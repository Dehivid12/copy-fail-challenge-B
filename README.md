## Hito 1: Reconocimiento del Entorno

Para empezar, verifiqué mi acceso a la máquina virtual vulnerable en QEMU. Ejecuté el comando `uname -a` para mostrar mi identificador único en el hostname y confirmar que la versión del kernel era la 6.12.0. Después, ejecuté `dmesg | grep PF_ALG` para comprobar que la familia de protocolos criptográficos requerida para el ataque estaba habilitada y registrada en el sistema.

---

## Hitos 2 y 3: Análisis de Entorno y Explotación

Me encontré con un fallo de infraestructura: el entorno minimalista de BusyBox no contaba con un intérprete de Python para ejecutar el PoC. En lugar de detener el análisis, realicé un proceso de inyección de dependencias para forzar la ejecución.

Primero, modifiqué el script de construcción (`03_build_rootfs.sh`) para inyectar una versión completa de Python (GNU). Como esta versión requería librerías dinámicas inexistentes en BusyBox, realicé un trasplante de dependencias copiando manualmente los cargadores y librerías desde el host hacia el rootfs:

- `libc.so.6`, `libm.so.6`, `libdl.so.2`, `libpthread.so.0` (Core de glibc)
- `libutil.so.1`, `librt.so.1`, `libz.so.1` (Dependencias en cadena de Python)
- `ld-linux-x86-64.so.2` (Cargador dinámico)

Al ejecutar el exploit descubrí una limitación arquitectónica crítica: el sistema se congeló (DoS silencioso). Esto demostró que al sobrescribir `/bin/su`, el exploit corrompió el archivo maestro `/bin/busybox` en el Page Cache, ya que en BusyBox todos los comandos son symlinks al mismo inode. Para resolverlo, compilé un binario SUID dedicado (`suid_target`) usando `musl-gcc` —no glibc, cuya inicialización fallaba con `: applet not found` en este entorno mínimo— y lo inyecté al rootfs como nuevo target del exploit. También corregí una fuga de file descriptors en la función `c()` del exploit, que agotaba el límite del kernel (`EMFILE`) al iterar sobre el payload sin cerrar sockets ni pipes.

Con ambas correcciones aplicadas, el resultado fue el siguiente:
~ $ /python/bin/python3 /exploit.py
[+] Page cache corrompido (1024 bytes escritos). Lanzando shell SUID...
/home/student # id
uid=0(root) gid=0(root) groups=1001(student)

---

## Hito 4: Mitigación (Parche del Kernel)

Para mitigar la vulnerabilidad, apliqué una corrección directamente en el código fuente del kernel de Linux. Modifiqué el archivo `kernel/linux/crypto/algif_aead.c`. Dentro de la función `_aead_recvmsg` (alrededor de la línea 282), reemplacé la variable de recepción (`rsgl_src`) por la de transmisión (`tsgl_src`). Generé la evidencia de esta mitigación utilizando `git diff`, la cual se encuentra adjunta en el archivo `hito4_mitigacion.patch` en la raíz de este repositorio.

---

## Bonus: Análisis de la Vulnerabilidad

La vulnerabilidad CVE-2026-31431 se debe a una falla lógica donde el origen y destino de una operación criptográfica AEAD compartían la misma referencia de memoria (operación in-place). Esto permitía, combinando el socket `AF_ALG` con la syscall `splice()`, forzar escrituras de datos directamente en el Page Cache de archivos de solo lectura sin permisos de escritura en disco. Al apuntar a un binario SUID, el payload se ejecutaba con privilegios de root. La solución implementada separa estrictamente los scatterlists de transmisión (TX) y recepción (RX), protegiendo la integridad del Page Cache.