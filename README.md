# VCXSRV MSYS2
- This is a tranplant version of vcxsrv open source code
- Replace with msys2 to solve compile problem in cygwin

# MSYS2 Compile Environemnt Intsall
```bash
pacman -S --noconfirm bison flex gawk gperf gzip nasm sed python
pacman -S --noconfirm diffutils texinfo help2man make gcc
pacman -S --noconfirm mingw64/mingw-w64-x86_64-nsis
pacman -S --noconfirm mingw64/mingw-w64-x86_64-python2 
pacman -S --noconfirm mingw64/mingw-w64-x86_64-python-mako
pacman -S --noconfirm mingw64/mingw-w64-x86_64-python-regex mingw64/mingw-w64-x86_64-python-lxml

# Only old version for bison can work 
wget http://ftp.gnu.org/gnu/bison/bison-3.0.3.tar.gz
tar zxvf bison-3.0.3.tar.gz
cd bison-3.0.3
./configure
make && make install

#link of msys2 is conflict with link of vs
mv /usr/bin/link /usr/bin/link-old
```

# Manual Operation For Package
- Modify Visual studio's Path in ./xorg-server/installer/packageall.bat

# Compile Binary
- Only x8864 version is verified
- You must install Microsoft Visual Studio 2019 First (Community Version as Well)
- Run [x64 Native Tools Command Prompt for VS 2019] and run this cmd
```bash
D:\msys64\msys2_shell.cmd -msys -use-full-path
```
- Run thiese cmds to compile package
```bash
export PATH=/mingw64/bin/:$PATH
bash buildadd.sh 1 9 D
```
