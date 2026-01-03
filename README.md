<body>

  <h1>NawOS</h1>
  <p><strong>NawOS</strong> is a simple operating system with basic shell commands and file management using a custom filesystem.</p>

  <h2>Prerequisites</h2>
  <blockquote>
    This project requires several build tools that are <strong>not preinstalled by default</strong>. Please install the following:
    <ul>
      <li><code>gcc</code></li>
      <li><code>make</code></li>
      <li><code>nasm</code></li>
      <li><code>qemu-system-i386</code></li>
    </ul>
    <p>If you are on <strong>Windows</strong>, it is highly recommended to use <strong>WSL (Windows Subsystem for Linux)</strong> or <strong>MSYS2</strong>.</p>
  </blockquote>

  <h2>Installation</h2>
  <h3>Ubuntu / Debian</h3>
  <pre><code>sudo apt update
sudo apt install build-essential nasm qemu-system make</code></pre>

  <h3>Arch Linux / Manjaro</h3>
  <pre><code>sudo pacman -S base-devel nasm qemu</code></pre>

  <h3>WSL (Windows Subsystem for Linux)</h3>
  <p>Install Ubuntu via Microsoft Store, then run the same <code>apt</code> commands as above.</p>

  <h2>Build and Run</h2>
  <pre><code>make clean && make && make run
</code></pre>

  <h2>Available Command</h2>
  <pre><code>help</code></pre>
  <p>Use <code>help</code> to view available commands in the NawOS shell.</p>

  <h2>Filesystem Features</h2>
  <ul>
    <li>Create, view, edit, and delete files using simple shell commands.</li>
    <li>Integrated text editor with minimal Vim-like behavior.</li>
    <li>Persistent file storage using a virtual disk image (<code>nawfs.img</code>).</li>
  </ul>

  
</body>
</html>
