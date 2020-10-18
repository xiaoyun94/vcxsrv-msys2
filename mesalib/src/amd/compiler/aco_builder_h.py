
template = """\
/*
 * Copyright (c) 2019 Valve Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 * This file was generated by aco_builder_h.py
 */

#ifndef _ACO_BUILDER_
#define _ACO_BUILDER_

#include "aco_ir.h"
#include "util/u_math.h"
#include "util/bitscan.h"

namespace aco {
enum dpp_ctrl {
    _dpp_quad_perm = 0x000,
    _dpp_row_sl = 0x100,
    _dpp_row_sr = 0x110,
    _dpp_row_rr = 0x120,
    dpp_wf_sl1 = 0x130,
    dpp_wf_rl1 = 0x134,
    dpp_wf_sr1 = 0x138,
    dpp_wf_rr1 = 0x13C,
    dpp_row_mirror = 0x140,
    dpp_row_half_mirror = 0x141,
    dpp_row_bcast15 = 0x142,
    dpp_row_bcast31 = 0x143
};

inline dpp_ctrl
dpp_quad_perm(unsigned lane0, unsigned lane1, unsigned lane2, unsigned lane3)
{
    assert(lane0 < 4 && lane1 < 4 && lane2 < 4 && lane3 < 4);
    return (dpp_ctrl)(lane0 | (lane1 << 2) | (lane2 << 4) | (lane3 << 6));
}

inline dpp_ctrl
dpp_row_sl(unsigned amount)
{
    assert(amount > 0 && amount < 16);
    return (dpp_ctrl)(((unsigned) _dpp_row_sl) | amount);
}

inline dpp_ctrl
dpp_row_sr(unsigned amount)
{
    assert(amount > 0 && amount < 16);
    return (dpp_ctrl)(((unsigned) _dpp_row_sr) | amount);
}

inline dpp_ctrl
dpp_row_rr(unsigned amount)
{
    assert(amount > 0 && amount < 16);
    return (dpp_ctrl)(((unsigned) _dpp_row_rr) | amount);
}

inline unsigned
ds_pattern_bitmode(unsigned and_mask, unsigned or_mask, unsigned xor_mask)
{
    assert(and_mask < 32 && or_mask < 32 && xor_mask < 32);
    return and_mask | (or_mask << 5) | (xor_mask << 10);
}

aco_ptr<Instruction> create_s_mov(Definition dst, Operand src);

extern uint8_t int8_mul_table[512];

enum sendmsg {
   sendmsg_none = 0,
   _sendmsg_gs = 2,
   _sendmsg_gs_done = 3,
   sendmsg_save_wave = 4,
   sendmsg_stall_wave_gen = 5,
   sendmsg_halt_waves = 6,
   sendmsg_ordered_ps_done = 7,
   sendmsg_early_prim_dealloc = 8,
   sendmsg_gs_alloc_req = 9,
   sendmsg_id_mask = 0xf,
};

inline sendmsg
sendmsg_gs(bool cut, bool emit, unsigned stream)
{
    assert(stream < 4);
    return (sendmsg)((unsigned)_sendmsg_gs | (cut << 4) | (emit << 5) | (stream << 8));
}

inline sendmsg
sendmsg_gs_done(bool cut, bool emit, unsigned stream)
{
    assert(stream < 4);
    return (sendmsg)((unsigned)_sendmsg_gs_done | (cut << 4) | (emit << 5) | (stream << 8));
}

class Builder {
public:
   struct Result {
      Instruction *instr;

      Result(Instruction *instr) : instr(instr) {}

      operator Instruction *() const {
         return instr;
      }

      operator Temp() const {
         return instr->definitions[0].getTemp();
      }

      operator Operand() const {
         return Operand((Temp)*this);
      }

      Definition& def(unsigned index) const {
         return instr->definitions[index];
      }

      aco_ptr<Instruction> get_ptr() const {
        return aco_ptr<Instruction>(instr);
      }
   };

   struct Op {
      Operand op;
      Op(Temp tmp) : op(tmp) {}
      Op(Operand op_) : op(op_) {}
      Op(Result res) : op((Temp)res) {}
   };

   enum WaveSpecificOpcode {
      s_cselect = (unsigned) aco_opcode::s_cselect_b64,
      s_cmp_lg = (unsigned) aco_opcode::s_cmp_lg_u64,
      s_and = (unsigned) aco_opcode::s_and_b64,
      s_andn2 = (unsigned) aco_opcode::s_andn2_b64,
      s_or = (unsigned) aco_opcode::s_or_b64,
      s_orn2 = (unsigned) aco_opcode::s_orn2_b64,
      s_not = (unsigned) aco_opcode::s_not_b64,
      s_mov = (unsigned) aco_opcode::s_mov_b64,
      s_wqm = (unsigned) aco_opcode::s_wqm_b64,
      s_and_saveexec = (unsigned) aco_opcode::s_and_saveexec_b64,
      s_or_saveexec = (unsigned) aco_opcode::s_or_saveexec_b64,
      s_xnor = (unsigned) aco_opcode::s_xnor_b64,
      s_xor = (unsigned) aco_opcode::s_xor_b64,
      s_bcnt1_i32 = (unsigned) aco_opcode::s_bcnt1_i32_b64,
      s_bitcmp1 = (unsigned) aco_opcode::s_bitcmp1_b64,
      s_ff1_i32 = (unsigned) aco_opcode::s_ff1_i32_b64,
      s_flbit_i32 = (unsigned) aco_opcode::s_flbit_i32_b64,
      s_lshl = (unsigned) aco_opcode::s_lshl_b64,
   };

   Program *program;
   bool use_iterator;
   bool start; // only when use_iterator == false
   RegClass lm;

   std::vector<aco_ptr<Instruction>> *instructions;
   std::vector<aco_ptr<Instruction>>::iterator it;
   bool is_precise = false;
   bool is_nuw = false;

   Builder(Program *pgm) : program(pgm), use_iterator(false), start(false), lm(pgm ? pgm->lane_mask : s2), instructions(NULL) {}
   Builder(Program *pgm, Block *block) : program(pgm), use_iterator(false), start(false), lm(pgm ? pgm->lane_mask : s2), instructions(&block->instructions) {}
   Builder(Program *pgm, std::vector<aco_ptr<Instruction>> *instrs) : program(pgm), use_iterator(false), start(false), lm(pgm ? pgm->lane_mask : s2), instructions(instrs) {}

   Builder precise() const {
      Builder res = *this;
      res.is_precise = true;
      return res;
   };

   Builder nuw() const {
      Builder res = *this;
      res.is_nuw = true;
      return res;
   }

   void moveEnd(Block *block) {
      instructions = &block->instructions;
   }

   void reset() {
      use_iterator = false;
      start = false;
      instructions = NULL;
   }

   void reset(Block *block) {
      use_iterator = false;
      start = false;
      instructions = &block->instructions;
   }

   void reset(std::vector<aco_ptr<Instruction>> *instrs) {
      use_iterator = false;
      start = false;
      instructions = instrs;
   }

   void reset(std::vector<aco_ptr<Instruction>> *instrs, std::vector<aco_ptr<Instruction>>::iterator instr_it) {
      use_iterator = true;
      start = false;
      instructions = instrs;
      it = instr_it;
   }

   Result insert(aco_ptr<Instruction> instr) {
      Instruction *instr_ptr = instr.get();
      if (instructions) {
         if (use_iterator) {
            it = instructions->emplace(it, std::move(instr));
            it = std::next(it);
         } else if (!start) {
            instructions->emplace_back(std::move(instr));
         } else {
            instructions->emplace(instructions->begin(), std::move(instr));
         }
      }
      return Result(instr_ptr);
   }

   Result insert(Instruction* instr) {
      if (instructions) {
         if (use_iterator) {
            it = instructions->emplace(it, aco_ptr<Instruction>(instr));
            it = std::next(it);
         } else if (!start) {
            instructions->emplace_back(aco_ptr<Instruction>(instr));
         } else {
            instructions->emplace(instructions->begin(), aco_ptr<Instruction>(instr));
         }
      }
      return Result(instr);
   }

   Temp tmp(RegClass rc) {
      return program->allocateTmp(rc);
   }

   Temp tmp(RegType type, unsigned size) {
      return tmp(RegClass(type, size));
   }

   Definition def(RegClass rc) {
      return Definition(program->allocateTmp(rc));
   }

   Definition def(RegType type, unsigned size) {
      return def(RegClass(type, size));
   }

   Definition def(RegClass rc, PhysReg reg) {
      return Definition(program->allocateId(rc), reg, rc);
   }

   inline aco_opcode w64or32(WaveSpecificOpcode opcode) const {
      if (program->wave_size == 64)
         return (aco_opcode) opcode;

      switch (opcode) {
      case s_cselect:
         return aco_opcode::s_cselect_b32;
      case s_cmp_lg:
         return aco_opcode::s_cmp_lg_u32;
      case s_and:
         return aco_opcode::s_and_b32;
      case s_andn2:
         return aco_opcode::s_andn2_b32;
      case s_or:
         return aco_opcode::s_or_b32;
      case s_orn2:
         return aco_opcode::s_orn2_b32;
      case s_not:
         return aco_opcode::s_not_b32;
      case s_mov:
         return aco_opcode::s_mov_b32;
      case s_wqm:
         return aco_opcode::s_wqm_b32;
      case s_and_saveexec:
         return aco_opcode::s_and_saveexec_b32;
      case s_or_saveexec:
         return aco_opcode::s_or_saveexec_b32;
      case s_xnor:
         return aco_opcode::s_xnor_b32;
      case s_xor:
         return aco_opcode::s_xor_b32;
      case s_bcnt1_i32:
         return aco_opcode::s_bcnt1_i32_b32;
      case s_bitcmp1:
         return aco_opcode::s_bitcmp1_b32;
      case s_ff1_i32:
         return aco_opcode::s_ff1_i32_b32;
      case s_flbit_i32:
         return aco_opcode::s_flbit_i32_b32;
      case s_lshl:
         return aco_opcode::s_lshl_b32;
      default:
         unreachable("Unsupported wave specific opcode.");
      }
   }

% for fixed in ['m0', 'vcc', 'exec', 'scc']:
   Operand ${fixed}(Temp tmp) {
       % if fixed == 'vcc' or fixed == 'exec':
          //vcc_hi and exec_hi can still be used in wave32
          assert(tmp.type() == RegType::sgpr && tmp.bytes() <= 8);
       % endif
       Operand op(tmp);
       op.setFixed(aco::${fixed});
       return op;
   }

   Definition ${fixed}(Definition def) {
       % if fixed == 'vcc' or fixed == 'exec':
          //vcc_hi and exec_hi can still be used in wave32
          assert(def.regClass().type() == RegType::sgpr && def.bytes() <= 8);
       % endif
       def.setFixed(aco::${fixed});
       return def;
   }

   Definition hint_${fixed}(Definition def) {
       % if fixed == 'vcc' or fixed == 'exec':
          //vcc_hi and exec_hi can still be used in wave32
          assert(def.regClass().type() == RegType::sgpr && def.bytes() <= 8);
       % endif
       def.setHint(aco::${fixed});
       return def;
   }

   Definition hint_${fixed}(RegClass rc) {
       return hint_${fixed}(def(rc));
   }

% endfor
   /* hand-written helpers */
   Temp as_uniform(Op op)
   {
      assert(op.op.isTemp());
      if (op.op.getTemp().type() == RegType::vgpr)
         return pseudo(aco_opcode::p_as_uniform, def(RegType::sgpr, op.op.size()), op);
      else
         return op.op.getTemp();
   }

   Result v_mul_imm(Definition dst, Temp tmp, uint32_t imm, bool bits24=false)
   {
      assert(tmp.type() == RegType::vgpr);
      if (imm == 0) {
         return vop1(aco_opcode::v_mov_b32, dst, Operand(0u));
      } else if (imm == 1) {
         return copy(dst, Operand(tmp));
      } else if (util_is_power_of_two_or_zero(imm)) {
         return vop2(aco_opcode::v_lshlrev_b32, dst, Operand((uint32_t)ffs(imm) - 1u), tmp);
      } else if (bits24) {
        return vop2(aco_opcode::v_mul_u32_u24, dst, Operand(imm), tmp);
      } else {
        Temp imm_tmp = copy(def(v1), Operand(imm));
        return vop3(aco_opcode::v_mul_lo_u32, dst, imm_tmp, tmp);
      }
   }

   Result v_mul24_imm(Definition dst, Temp tmp, uint32_t imm)
   {
      return v_mul_imm(dst, tmp, imm, true);
   }

   Result copy(Definition dst, Op op_) {
      Operand op = op_.op;
      assert(op.bytes() == dst.bytes());
      if (dst.regClass() == s1 && op.size() == 1 && op.isLiteral()) {
         uint32_t imm = op.constantValue();
         if (imm == 0x3e22f983) {
            if (program->chip_class >= GFX8)
               op.setFixed(PhysReg{248}); /* it can be an inline constant on GFX8+ */
         } else if (imm >= 0xffff8000 || imm <= 0x7fff) {
            return sopk(aco_opcode::s_movk_i32, dst, imm & 0xFFFFu);
         } else if (util_bitreverse(imm) <= 64 || util_bitreverse(imm) >= 0xFFFFFFF0) {
            uint32_t rev = util_bitreverse(imm);
            return dst.regClass() == v1 ?
                   vop1(aco_opcode::v_bfrev_b32, dst, Operand(rev)) :
                   sop1(aco_opcode::s_brev_b32, dst, Operand(rev));
         } else if (imm != 0) {
            unsigned start = (ffs(imm) - 1) & 0x1f;
            unsigned size = util_bitcount(imm) & 0x1f;
            if ((((1u << size) - 1u) << start) == imm)
                return sop2(aco_opcode::s_bfm_b32, dst, Operand(size), Operand(start));
         }
      }

      if (dst.regClass() == s1) {
        return sop1(aco_opcode::s_mov_b32, dst, op);
      } else if (dst.regClass() == s2) {
        return sop1(aco_opcode::s_mov_b64, dst, op);
      } else if (dst.regClass() == v1 || dst.regClass() == v1.as_linear()) {
        return vop1(aco_opcode::v_mov_b32, dst, op);
      } else if (op.bytes() > 2 || (op.isLiteral() && dst.regClass().is_subdword())) {
         return pseudo(aco_opcode::p_create_vector, dst, op);
      } else if (op.bytes() == 1 && op.isConstant()) {
        uint8_t val = op.constantValue();
        Operand op32((uint32_t)val | (val & 0x80u ? 0xffffff00u : 0u));
        aco_ptr<SDWA_instruction> sdwa;
        if (op32.isLiteral()) {
            sdwa.reset(create_instruction<SDWA_instruction>(aco_opcode::v_mul_u32_u24, asSDWA(Format::VOP2), 2, 1));
            uint32_t a = (uint32_t)int8_mul_table[val * 2];
            uint32_t b = (uint32_t)int8_mul_table[val * 2 + 1];
            sdwa->operands[0] = Operand(a | (a & 0x80u ? 0xffffff00u : 0x0u));
            sdwa->operands[1] = Operand(b | (b & 0x80u ? 0xffffff00u : 0x0u));
        } else {
            sdwa.reset(create_instruction<SDWA_instruction>(aco_opcode::v_mov_b32, asSDWA(Format::VOP1), 1, 1));
            sdwa->operands[0] = op32;
        }
        sdwa->definitions[0] = dst;
        sdwa->sel[0] = sdwa_udword;
        sdwa->sel[1] = sdwa_udword;
        sdwa->dst_sel = sdwa_ubyte;
        sdwa->dst_preserve = true;
        return insert(std::move(sdwa));
      } else if (op.bytes() == 2 && op.isConstant() && !op.isLiteral()) {
        aco_ptr<SDWA_instruction> sdwa{create_instruction<SDWA_instruction>(aco_opcode::v_add_f16, asSDWA(Format::VOP2), 2, 1)};
        sdwa->operands[0] = op;
        sdwa->operands[1] = Operand(0u);
        sdwa->definitions[0] = dst;
        sdwa->sel[0] = sdwa_uword;
        sdwa->sel[1] = sdwa_udword;
        sdwa->dst_sel = dst.bytes() == 1 ? sdwa_ubyte : sdwa_uword;
        sdwa->dst_preserve = true;
        return insert(std::move(sdwa));
      } else if (dst.regClass().is_subdword()) {
        if (program->chip_class >= GFX8) {
            aco_ptr<SDWA_instruction> sdwa{create_instruction<SDWA_instruction>(aco_opcode::v_mov_b32, asSDWA(Format::VOP1), 1, 1)};
            sdwa->operands[0] = op;
            sdwa->definitions[0] = dst;
            sdwa->sel[0] = op.bytes() == 1 ? sdwa_ubyte : sdwa_uword;
            sdwa->dst_sel = dst.bytes() == 1 ? sdwa_ubyte : sdwa_uword;
            sdwa->dst_preserve = true;
            return insert(std::move(sdwa));
        } else {
            return vop1(aco_opcode::v_mov_b32, dst, op);
        }
      } else {
        unreachable("Unhandled case in bld.copy()");
      }
   }

   Result vadd32(Definition dst, Op a, Op b, bool carry_out=false, Op carry_in=Op(Operand(s2)), bool post_ra=false) {
      if (!b.op.isTemp() || b.op.regClass().type() != RegType::vgpr)
         std::swap(a, b);
      assert((post_ra || b.op.hasRegClass()) && b.op.regClass().type() == RegType::vgpr);

      if (!carry_in.op.isUndefined())
         return vop2(aco_opcode::v_addc_co_u32, Definition(dst), hint_vcc(def(lm)), a, b, carry_in);
      else if (program->chip_class >= GFX10 && carry_out)
         return vop3(aco_opcode::v_add_co_u32_e64, Definition(dst), def(lm), a, b);
      else if (program->chip_class < GFX9 || carry_out)
         return vop2(aco_opcode::v_add_co_u32, Definition(dst), hint_vcc(def(lm)), a, b);
      else
         return vop2(aco_opcode::v_add_u32, Definition(dst), a, b);
   }

   Result vsub32(Definition dst, Op a, Op b, bool carry_out=false, Op borrow=Op(Operand(s2)))
   {
      if (!borrow.op.isUndefined() || program->chip_class < GFX9)
         carry_out = true;

      bool reverse = !b.op.isTemp() || b.op.regClass().type() != RegType::vgpr;
      if (reverse)
         std::swap(a, b);
      assert(b.op.isTemp() && b.op.regClass().type() == RegType::vgpr);

      aco_opcode op;
      Temp carry;
      if (carry_out) {
         carry = tmp(s2);
         if (borrow.op.isUndefined())
            op = reverse ? aco_opcode::v_subrev_co_u32 : aco_opcode::v_sub_co_u32;
         else
            op = reverse ? aco_opcode::v_subbrev_co_u32 : aco_opcode::v_subb_co_u32;
      } else {
         op = reverse ? aco_opcode::v_subrev_u32 : aco_opcode::v_sub_u32;
      }
      bool vop3 = false;
      if (program->chip_class >= GFX10 && op == aco_opcode::v_subrev_co_u32) {
        vop3 = true;
        op = aco_opcode::v_subrev_co_u32_e64;
      } else if (program->chip_class >= GFX10 && op == aco_opcode::v_sub_co_u32) {
        vop3 = true;
        op = aco_opcode::v_sub_co_u32_e64;
      }

      int num_ops = borrow.op.isUndefined() ? 2 : 3;
      int num_defs = carry_out ? 2 : 1;
      aco_ptr<Instruction> sub;
      if (vop3)
        sub.reset(create_instruction<VOP3A_instruction>(op, Format::VOP3B, num_ops, num_defs));
      else
        sub.reset(create_instruction<VOP2_instruction>(op, Format::VOP2, num_ops, num_defs));
      sub->operands[0] = a.op;
      sub->operands[1] = b.op;
      if (!borrow.op.isUndefined())
         sub->operands[2] = borrow.op;
      sub->definitions[0] = dst;
      if (carry_out) {
         sub->definitions[1] = Definition(carry);
         sub->definitions[1].setHint(aco::vcc);
      }
      return insert(std::move(sub));
   }

   Result readlane(Definition dst, Op vsrc, Op lane)
   {
      if (program->chip_class >= GFX8)
         return vop3(aco_opcode::v_readlane_b32_e64, dst, vsrc, lane);
      else
         return vop2(aco_opcode::v_readlane_b32, dst, vsrc, lane);
   }
   Result writelane(Definition dst, Op val, Op lane, Op vsrc) {
      if (program->chip_class >= GFX8)
         return vop3(aco_opcode::v_writelane_b32_e64, dst, val, lane, vsrc);
      else
         return vop2(aco_opcode::v_writelane_b32, dst, val, lane, vsrc);
   }
<%
import itertools
formats = [("pseudo", [Format.PSEUDO], 'Pseudo_instruction', list(itertools.product(range(5), range(5))) + [(8, 1), (1, 8)]),
           ("sop1", [Format.SOP1], 'SOP1_instruction', [(0, 1), (1, 0), (1, 1), (2, 1), (3, 2)]),
           ("sop2", [Format.SOP2], 'SOP2_instruction', itertools.product([1, 2], [2, 3])),
           ("sopk", [Format.SOPK], 'SOPK_instruction', itertools.product([0, 1, 2], [0, 1])),
           ("sopp", [Format.SOPP], 'SOPP_instruction', itertools.product([0, 1], [0, 1])),
           ("sopc", [Format.SOPC], 'SOPC_instruction', [(1, 2)]),
           ("smem", [Format.SMEM], 'SMEM_instruction', [(0, 4), (0, 3), (1, 0), (1, 3), (1, 2), (0, 0)]),
           ("ds", [Format.DS], 'DS_instruction', [(1, 1), (1, 2), (0, 3), (0, 4)]),
           ("mubuf", [Format.MUBUF], 'MUBUF_instruction', [(0, 4), (1, 3)]),
           ("mtbuf", [Format.MTBUF], 'MTBUF_instruction', [(0, 4), (1, 3)]),
           ("mimg", [Format.MIMG], 'MIMG_instruction', [(0, 3), (1, 3)]),
           ("exp", [Format.EXP], 'Export_instruction', [(0, 4)]),
           ("branch", [Format.PSEUDO_BRANCH], 'Pseudo_branch_instruction', itertools.product([1], [0, 1])),
           ("barrier", [Format.PSEUDO_BARRIER], 'Pseudo_barrier_instruction', [(0, 0)]),
           ("reduction", [Format.PSEUDO_REDUCTION], 'Pseudo_reduction_instruction', [(3, 2)]),
           ("vop1", [Format.VOP1], 'VOP1_instruction', [(0, 0), (1, 1), (2, 2)]),
           ("vop2", [Format.VOP2], 'VOP2_instruction', itertools.product([1, 2], [2, 3])),
           ("vop2_sdwa", [Format.VOP2, Format.SDWA], 'SDWA_instruction', itertools.product([1, 2], [2, 3])),
           ("vopc", [Format.VOPC], 'VOPC_instruction', itertools.product([1, 2], [2])),
           ("vop3", [Format.VOP3A], 'VOP3A_instruction', [(1, 3), (1, 2), (1, 1), (2, 2)]),
           ("vintrp", [Format.VINTRP], 'Interp_instruction', [(1, 2), (1, 3)]),
           ("vop1_dpp", [Format.VOP1, Format.DPP], 'DPP_instruction', [(1, 1)]),
           ("vop2_dpp", [Format.VOP2, Format.DPP], 'DPP_instruction', itertools.product([1, 2], [2, 3])),
           ("vopc_dpp", [Format.VOPC, Format.DPP], 'DPP_instruction', itertools.product([1, 2], [2])),
           ("vop1_e64", [Format.VOP1, Format.VOP3A], 'VOP3A_instruction', itertools.product([1], [1])),
           ("vop2_e64", [Format.VOP2, Format.VOP3A], 'VOP3A_instruction', itertools.product([1, 2], [2, 3])),
           ("vopc_e64", [Format.VOPC, Format.VOP3A], 'VOP3A_instruction', itertools.product([1, 2], [2])),
           ("flat", [Format.FLAT], 'FLAT_instruction', [(0, 3), (1, 2)]),
           ("global", [Format.GLOBAL], 'FLAT_instruction', [(0, 3), (1, 2)])]
formats = [(f if len(f) == 5 else f + ('',)) for f in formats]
%>\\
% for name, formats, struct, shapes, extra_field_setup in formats:
    % for num_definitions, num_operands in shapes:
        <%
        args = ['aco_opcode opcode']
        for i in range(num_definitions):
            args.append('Definition def%d' % i)
        for i in range(num_operands):
            args.append('Op op%d' % i)
        for f in formats:
            args += f.get_builder_field_decls()
        %>\\

   Result ${name}(${', '.join(args)})
   {
      ${struct} *instr = create_instruction<${struct}>(opcode, (Format)(${'|'.join('(int)Format::%s' % f.name for f in formats)}), ${num_operands}, ${num_definitions});
        % for i in range(num_definitions):
            instr->definitions[${i}] = def${i};
            instr->definitions[${i}].setPrecise(is_precise);
            instr->definitions[${i}].setNUW(is_nuw);
        % endfor
        % for i in range(num_operands):
            instr->operands[${i}] = op${i}.op;
        % endfor
        % for f in formats:
            % for dest, field_name in zip(f.get_builder_field_dests(), f.get_builder_field_names()):
      instr->${dest} = ${field_name};
            % endfor
            ${f.get_builder_initialization(num_operands)}
        % endfor
       ${extra_field_setup}
      return insert(instr);
   }

    % if name == 'sop1' or name == 'sop2' or name == 'sopc':
        <%
        args[0] = 'WaveSpecificOpcode opcode'
        params = []
        for i in range(num_definitions):
            params.append('def%d' % i)
        for i in range(num_operands):
            params.append('op%d' % i)
        %>\\

   inline Result ${name}(${', '.join(args)})
   {
       return ${name}(w64or32(opcode), ${', '.join(params)});
   }

    % endif
    % endfor
% endfor
};

}
#endif /* _ACO_BUILDER_ */"""

from aco_opcodes import opcodes, Format
from mako.template import Template

print(Template(template).render(opcodes=opcodes, Format=Format))
