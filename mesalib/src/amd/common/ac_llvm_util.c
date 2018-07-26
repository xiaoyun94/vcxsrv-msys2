/*
 * Copyright 2014 Advanced Micro Devices, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE COPYRIGHT HOLDERS, AUTHORS AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 */
/* based on pieces from si_pipe.c and radeon_llvm_emit.c */
#include "ac_llvm_util.h"
#include "ac_llvm_build.h"
#include "util/bitscan.h"
#include <llvm-c/Core.h>
#include <llvm-c/Support.h>
#include <llvm-c/Transforms/IPO.h>
#include <llvm-c/Transforms/Scalar.h>
#if HAVE_LLVM >= 0x0700
#include <llvm-c/Transforms/Utils.h>
#endif
#include "c11/threads.h"
#include "gallivm/lp_bld_misc.h"
#include "util/u_math.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void ac_init_llvm_target()
{
	LLVMInitializeAMDGPUTargetInfo();
	LLVMInitializeAMDGPUTarget();
	LLVMInitializeAMDGPUTargetMC();
	LLVMInitializeAMDGPUAsmPrinter();

	/* For inline assembly. */
	LLVMInitializeAMDGPUAsmParser();

	/* Workaround for bug in llvm 4.0 that causes image intrinsics
	 * to disappear.
	 * https://reviews.llvm.org/D26348
	 *
	 * "mesa" is the prefix for error messages.
	 *
	 * -global-isel-abort=2 is a no-op unless global isel has been enabled.
	 * This option tells the backend to fall-back to SelectionDAG and print
	 * a diagnostic message if global isel fails.
	 */
	const char *argv[3] = { "mesa", "-simplifycfg-sink-common=false", "-global-isel-abort=2" };
	LLVMParseCommandLineOptions(3, argv, NULL);
}

static once_flag ac_init_llvm_target_once_flag = ONCE_FLAG_INIT;

void ac_init_llvm_once(void)
{
	call_once(&ac_init_llvm_target_once_flag, ac_init_llvm_target);
}

static LLVMTargetRef ac_get_llvm_target(const char *triple)
{
	LLVMTargetRef target = NULL;
	char *err_message = NULL;

	if (LLVMGetTargetFromTriple(triple, &target, &err_message)) {
		fprintf(stderr, "Cannot find target for triple %s ", triple);
		if (err_message) {
			fprintf(stderr, "%s\n", err_message);
		}
		LLVMDisposeMessage(err_message);
		return NULL;
	}
	return target;
}

const char *ac_get_llvm_processor_name(enum radeon_family family)
{
	switch (family) {
	case CHIP_TAHITI:
		return "tahiti";
	case CHIP_PITCAIRN:
		return "pitcairn";
	case CHIP_VERDE:
		return "verde";
	case CHIP_OLAND:
		return "oland";
	case CHIP_HAINAN:
		return "hainan";
	case CHIP_BONAIRE:
		return "bonaire";
	case CHIP_KABINI:
		return "kabini";
	case CHIP_KAVERI:
		return "kaveri";
	case CHIP_HAWAII:
		return "hawaii";
	case CHIP_MULLINS:
		return "mullins";
	case CHIP_TONGA:
		return "tonga";
	case CHIP_ICELAND:
		return "iceland";
	case CHIP_CARRIZO:
		return "carrizo";
	case CHIP_FIJI:
		return "fiji";
	case CHIP_STONEY:
		return "stoney";
	case CHIP_POLARIS10:
		return "polaris10";
	case CHIP_POLARIS11:
	case CHIP_POLARIS12:
	case CHIP_VEGAM:
		return "polaris11";
	case CHIP_VEGA10:
		return "gfx900";
	case CHIP_RAVEN:
		return "gfx902";
	case CHIP_VEGA12:
		return HAVE_LLVM >= 0x0700 ? "gfx904" : "gfx902";
	case CHIP_VEGA20:
		return HAVE_LLVM >= 0x0700 ? "gfx906" : "gfx902";
	default:
		return "";
	}
}

static LLVMTargetMachineRef ac_create_target_machine(enum radeon_family family,
						     enum ac_target_machine_options tm_options,
						     const char **out_triple)
{
	assert(family >= CHIP_TAHITI);
	char features[256];
	const char *triple = (tm_options & AC_TM_SUPPORTS_SPILL) ? "amdgcn-mesa-mesa3d" : "amdgcn--";
	LLVMTargetRef target = ac_get_llvm_target(triple);
	bool barrier_does_waitcnt = family != CHIP_VEGA20;

	snprintf(features, sizeof(features),
		 "+DumpCode,+vgpr-spilling,-fp32-denormals,+fp64-denormals%s%s%s%s%s",
		 tm_options & AC_TM_SISCHED ? ",+si-scheduler" : "",
		 tm_options & AC_TM_FORCE_ENABLE_XNACK ? ",+xnack" : "",
		 tm_options & AC_TM_FORCE_DISABLE_XNACK ? ",-xnack" : "",
		 tm_options & AC_TM_PROMOTE_ALLOCA_TO_SCRATCH ? ",-promote-alloca" : "",
		 barrier_does_waitcnt ? ",+auto-waitcnt-before-barrier" : "");
	
	LLVMTargetMachineRef tm = LLVMCreateTargetMachine(
	                             target,
	                             triple,
	                             ac_get_llvm_processor_name(family),
				     features,
	                             LLVMCodeGenLevelDefault,
	                             LLVMRelocDefault,
	                             LLVMCodeModelDefault);

	if (out_triple)
		*out_triple = triple;
	if (tm_options & AC_TM_ENABLE_GLOBAL_ISEL)
		ac_enable_global_isel(tm);
	return tm;
}

static LLVMPassManagerRef ac_create_passmgr(LLVMTargetLibraryInfoRef target_library_info,
					    bool check_ir)
{
	LLVMPassManagerRef passmgr = LLVMCreatePassManager();
	if (!passmgr)
		return NULL;

	if (target_library_info)
		LLVMAddTargetLibraryInfo(target_library_info,
					 passmgr);

	if (check_ir)
		LLVMAddVerifierPass(passmgr);
	LLVMAddAlwaysInlinerPass(passmgr);
	/* Normally, the pass manager runs all passes on one function before
	 * moving onto another. Adding a barrier no-op pass forces the pass
	 * manager to run the inliner on all functions first, which makes sure
	 * that the following passes are only run on the remaining non-inline
	 * function, so it removes useless work done on dead inline functions.
	 */
	ac_llvm_add_barrier_noop_pass(passmgr);
	/* This pass should eliminate all the load and store instructions. */
	LLVMAddPromoteMemoryToRegisterPass(passmgr);
	LLVMAddScalarReplAggregatesPass(passmgr);
	LLVMAddLICMPass(passmgr);
	LLVMAddAggressiveDCEPass(passmgr);
	LLVMAddCFGSimplificationPass(passmgr);
	/* This is recommended by the instruction combining pass. */
	LLVMAddEarlyCSEMemSSAPass(passmgr);
	LLVMAddInstructionCombiningPass(passmgr);
	return passmgr;
}

static const char *attr_to_str(enum ac_func_attr attr)
{
   switch (attr) {
   case AC_FUNC_ATTR_ALWAYSINLINE: return "alwaysinline";
   case AC_FUNC_ATTR_INREG: return "inreg";
   case AC_FUNC_ATTR_NOALIAS: return "noalias";
   case AC_FUNC_ATTR_NOUNWIND: return "nounwind";
   case AC_FUNC_ATTR_READNONE: return "readnone";
   case AC_FUNC_ATTR_READONLY: return "readonly";
   case AC_FUNC_ATTR_WRITEONLY: return "writeonly";
   case AC_FUNC_ATTR_INACCESSIBLE_MEM_ONLY: return "inaccessiblememonly";
   case AC_FUNC_ATTR_CONVERGENT: return "convergent";
   default:
	   fprintf(stderr, "Unhandled function attribute: %x\n", attr);
	   return 0;
   }
}

void
ac_add_function_attr(LLVMContextRef ctx, LLVMValueRef function,
                     int attr_idx, enum ac_func_attr attr)
{
   const char *attr_name = attr_to_str(attr);
   unsigned kind_id = LLVMGetEnumAttributeKindForName(attr_name,
                                                      strlen(attr_name));
   LLVMAttributeRef llvm_attr = LLVMCreateEnumAttribute(ctx, kind_id, 0);

   if (LLVMIsAFunction(function))
      LLVMAddAttributeAtIndex(function, attr_idx, llvm_attr);
   else
      LLVMAddCallSiteAttribute(function, attr_idx, llvm_attr);
}

void ac_add_func_attributes(LLVMContextRef ctx, LLVMValueRef function,
			    unsigned attrib_mask)
{
	attrib_mask |= AC_FUNC_ATTR_NOUNWIND;
	attrib_mask &= ~AC_FUNC_ATTR_LEGACY;

	while (attrib_mask) {
		enum ac_func_attr attr = 1u << u_bit_scan(&attrib_mask);
		ac_add_function_attr(ctx, function, -1, attr);
	}
}

void
ac_dump_module(LLVMModuleRef module)
{
	char *str = LLVMPrintModuleToString(module);
	fprintf(stderr, "%s", str);
	LLVMDisposeMessage(str);
}

void
ac_llvm_add_target_dep_function_attr(LLVMValueRef F,
				     const char *name, unsigned value)
{
	char str[16];

	snprintf(str, sizeof(str), "0x%x", value);
	LLVMAddTargetDependentFunctionAttr(F, name, str);
}

unsigned
ac_count_scratch_private_memory(LLVMValueRef function)
{
	unsigned private_mem_vgprs = 0;

	/* Process all LLVM instructions. */
	LLVMBasicBlockRef bb = LLVMGetFirstBasicBlock(function);
	while (bb) {
		LLVMValueRef next = LLVMGetFirstInstruction(bb);

		while (next) {
			LLVMValueRef inst = next;
			next = LLVMGetNextInstruction(next);

			if (LLVMGetInstructionOpcode(inst) != LLVMAlloca)
				continue;

			LLVMTypeRef type = LLVMGetElementType(LLVMTypeOf(inst));
			/* No idea why LLVM aligns allocas to 4 elements. */
			unsigned alignment = LLVMGetAlignment(inst);
			unsigned dw_size = align(ac_get_type_size(type) / 4, alignment);
			private_mem_vgprs += dw_size;
		}
		bb = LLVMGetNextBasicBlock(bb);
	}

	return private_mem_vgprs;
}

bool
ac_init_llvm_compiler(struct ac_llvm_compiler *compiler,
		      bool okay_to_leak_target_library_info,
		      enum radeon_family family,
		      enum ac_target_machine_options tm_options)
{
	const char *triple;
	memset(compiler, 0, sizeof(*compiler));

	compiler->tm = ac_create_target_machine(family,
					    tm_options, &triple);
	if (!compiler->tm)
		return false;

	if (okay_to_leak_target_library_info || (HAVE_LLVM >= 0x0700)) {
		compiler->target_library_info =
			ac_create_target_library_info(triple);
		if (!compiler->target_library_info)
			goto fail;
	}

	compiler->passmgr = ac_create_passmgr(compiler->target_library_info,
					      tm_options & AC_TM_CHECK_IR);
	if (!compiler->passmgr)
		goto fail;

	return true;
fail:
	ac_destroy_llvm_compiler(compiler);
	return false;
}

void
ac_destroy_llvm_compiler(struct ac_llvm_compiler *compiler)
{
	if (compiler->passmgr)
		LLVMDisposePassManager(compiler->passmgr);
#if HAVE_LLVM >= 0x0700
	/* This crashes on LLVM 5.0 and 6.0 and Ubuntu 18.04, so leak it there. */
	if (compiler->target_library_info)
		ac_dispose_target_library_info(compiler->target_library_info);
#endif
	if (compiler->tm)
		LLVMDisposeTargetMachine(compiler->tm);
}
