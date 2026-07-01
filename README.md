<body>

  <h1>NawOS</h1>

  <p>
    NawOS is a hobby operating system for x86 written in C and Assembly.
    The project started as a small educational kernel and gradually evolved into a modular monolithic operating system with its own shell, filesystem, networking stack, text editor, and bytecode virtual machine.
  </p>

  <p>
    The system runs in 32-bit protected mode and is designed to work inside QEMU.
  </p>

  <hr>

  <h1>Features</h1>

  <h2>Boot and Kernel</h2>

  <ul>
    <li>Custom x86 bootloader</li>
    <li>Transition from Real Mode to 32-bit Protected Mode</li>
    <li>GDT and IDT setup</li>
    <li>IRQ and interrupt handling</li>
    <li>VGA text mode terminal</li>
    <li>PS/2 keyboard driver</li>
    <li>Modular kernel architecture</li>
  </ul>

  <hr>

  <h2>Filesystem — NawFS</h2>

  <p>
    NawOS uses its own custom filesystem called <strong>NawFS</strong>.
  </p>

  <p>Features:</p>

  <ul>
    <li>Persistent storage</li>
    <li>File creation and deletion</li>
    <li>File reading and writing</li>
    <li>Automatic filesystem formatting on first boot</li>
    <li>Simple sector-based file allocation</li>
  </ul>

  <hr>

  <h2>Shell</h2>

  <p>
    Built-in shell with command parsing and dispatch system.
  </p>

  <p>Example commands:</p>

  <pre><code>help
ls
cat
edit
rm
calc
ipconfig
dhcp
run
clear
reboot</code></pre>

  <hr>

  <h2>Text Editor</h2>

  <p>
    Integrated terminal text editor with Vim-inspired controls.
  </p>

  <p>Features:</p>

  <ul>
    <li>Cursor movement</li>
    <li>Text insertion and deletion</li>
    <li>File saving/loading</li>
    <li>Multi-line editing</li>
    <li>Scroll support</li>
  </ul>

  <hr>

  <h2>Networking</h2>

  <p>
    Basic network stack implementation with:
  </p>

  <ul>
    <li>PCI device scanning</li>
    <li>RTL8139 network driver</li>
    <li>Ethernet frame handling</li>
    <li>IPv4 support</li>
    <li>UDP packets</li>
    <li>DHCP client</li>
  </ul>

  <p>
    Networking is designed primarily for educational and experimental purposes.
  </p>

  <hr>

  <h2>Lelya VM</h2>

  <p>
    NawOS includes a simple stack-based bytecode virtual machine.
  </p>

  <p>Features:</p>

  <ul>
    <li>Integer operations</li>
    <li>Stack instructions</li>
    <li>Arithmetic execution</li>
    <li>Basic interpreter architecture</li>
  </ul>

  <hr>

  <h1>Project Structure</h1>

  <pre><code>boot/               Bootloader and protected mode transition
kernel/             Core kernel systems
drivers/            Hardware drivers
fs/                 NawFS filesystem
net/                Network stack
apps/               Built-in applications
apps/editor/        Text editor
apps/nawlang/       Virtual machine and language runtime</code></pre>

  <hr>

  <h1>Requirements</h1>

  <p>The following tools are required:</p>

  <ul>
    <li>gcc</li>
    <li>make</li>
    <li>nasm</li>
    <li>ld</li>
    <li>qemu-system-i386</li>
  </ul>

  <hr>

  <h1>Installation</h1>

  <h2>Ubuntu / Debian</h2>

  <pre><code>sudo apt update
sudo apt install build-essential nasm qemu-system-x86 make</code></pre>

  <hr>

  <h2>Arch Linux</h2>

  <pre><code>sudo pacman -S base-devel nasm qemu</code></pre>

  <hr>

  <h2>Windows</h2>

  <p>It is recommended to use:</p>

  <ul>
    <li>WSL2</li>
    <li>MSYS2</li>
  </ul>

  <p>
    WSL2 with Ubuntu is the preferred environment.
  </p>

  <hr>

  <h1>Build</h1>

  <pre><code>make clean
make</code></pre>

  <hr>

  <h1>Run</h1>

  <pre><code>make run</code></pre>

  <hr>

  <h1>Running with Persistent Filesystem</h1>

  <p>
    NawOS can use a separate filesystem image:
  </p>

  <pre><code>qemu-system-i386 \
-drive file=os-image.bin,format=raw,index=0,if=floppy \
-drive file=nawfs.img,format=raw,index=1,if=ide \
-net nic,model=rtl8139 \
-net user</code></pre>

  <hr>

  <h1>Technical Details</h1>

  <ul>
    <li><strong>Architecture:</strong> x86 (32-bit)</li>
    <li><strong>Language:</strong> C + NASM Assembly</li>
    <li><strong>Boot mode:</strong> BIOS</li>
    <li><strong>Execution mode:</strong> Protected Mode</li>
    <li><strong>Memory model:</strong> Flat memory model</li>
    <li><strong>Filesystem:</strong> NawFS</li>
    <li><strong>Network card:</strong> RTL8139</li>
  </ul>

  <hr>

  <h1>Goals</h1>


  <p>
    The project is intentionally written without external libraries or existing kernels.
  </p>


  <hr>

  <h1>License</h1>

  <p> GPL-3.0 license</p>

</body>
