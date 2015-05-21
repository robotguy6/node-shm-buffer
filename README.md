# shm-buffer

shared memory segment bindings for node.js that work in 0.10+

## Installing

    npm install shm-buffer

## Usage

    shm = require("shm-buffer")
    buffer = shm.open(key, flags, mode, size)
    buffer.delete() // deletes the shared memory region

<table>
  <tr>
    <td><i>key</i></td>
    <td>System's id for the shared memory block.</td>
  </tr>
  <tr>
    <td><i>flags</i></td>
    <td><ul>
        <li>"a" for access (sets SHM_RDONLY for shmat) use this flag when you need to open an existing shared memory segment for read only</li>
        <li>"c" for create (sets IPC_CREATE) use this flag when you need to create a new shared memory segment or if a segment with the same key exists, try to open it for read and write</li>
        <li>"w" for read &amp; write access use this flag when you need to read and write to a shared memory segment, use this flag in most cases.</li>
        <li>"n" create a new memory segment (sets IPC_CREATE|IPC_EXCL) use this flag when you want to create a new shared memory segment but if one already exists with the same flag, fail. This is useful for security purposes, using this you can prevent race condition exploits.</li>
  </tr>
  <tr>
    <td><i>mode</i></td>
    <td>The permissions that you wish to assign to your memory segment, those are the same as permission for a file. Permissions need to be passed in octal form, like for example 0644.</td>
  </tr>
  <tr>
    <td><i>size</i></td>
    <td>The size of the shared memory block you wish to create in bytes.</td>
  </tr>
</table>

## See Also

* http://linux.die.net/man/2/shmget

## Inspired by

* https://github.com/vpj/node_shm
* http://php.net/manual/en/ref.shmop.php
* https://github.com/php/php-src/blob/master/ext/shmop/shmop.c#L145
* https://github.com/geocar/mmap/blob/master/mmap.cpp
