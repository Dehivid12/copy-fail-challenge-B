# Copy Fail Lab — CVE-2026-31431 (v2)

Devcontainer reproducible para experimentar con la vulnerabilidad **Copy Fail**
(CVE-2026-31431) en un kernel Linux 6.12 controlado dentro de QEMU.

Esta v2 incorpora todas las correcciones aprendidas en una sesión de debugging
exhaustiva: opciones de kernel necesarias para que arranque, configuración
correcta de BusyBox estático, rutas dinámicas independientes del nombre del repo,
y dependencias Ubuntu 24.04 corregidas.

---

## Inicio rápido para el estudiante

1. Abre un Codespace desde este repo.
   ```bash
   #CONFIGURACION DE EJEMPLO!!!!!!!!!!!
   apt update
   apt install gh
   
   gh api user --jq '"\(.name) → \(.email // .login)"'
   
   git config --global user.name "Jonathan E. Tito O."
   git config --global user.email "jonathantito@users.noreply.github.com"
   git config --global --add safe.directory /workspaces/copy-fail-challenge-1
   make setup
   ```
3. Configura tu identidad git:
   ```bash
   git config --global user.name "Tu Nombre"
   git config --global user.email "tu@correo.com"
   ```
4. Ejecuta:
   ```bash
   make setup    # descarga kernel + arma rootfs (~5 min)
   make qemu     # arranca la VM vulnerable
   ```

Para salir de QEMU: `Ctrl+A` luego `X`.

---

## Configuración inicial del docente (una sola vez)

### 1. Subir este repo a GitHub

```bash
cd copyfail-v2
git init && git add -A && git commit -m "initial"
git branch -M main
gh repo create TU-ORG/copy-fail-lab --public --source=. --push
```

### 2. Marcarlo como Template

GitHub → tu repo → Settings → marcar `Template repository`.

### 3. Editar `.devcontainer/devcontainer.json`

Cambia el valor `KERNEL_REPO`:
```json
"KERNEL_REPO": "TU-ORG/copy-fail-lab"
```

Commit y push.

### 4. Disparar el workflow del kernel

GitHub → Actions → `Build Vulnerable Kernel` → Run workflow.
Tarda ~25 min en los servidores de GitHub (no en tu Codespace).
Al terminar crea un Release con el `bzImage_vuln` listo para descarga.

### 5. Verificar

Tu repo → Releases → debe aparecer `kernel-v6.12-vuln` con tres archivos
adjuntos. Los estudiantes ahora pueden hacer `make setup` y descarga en 2 min.

---

## Estructura del repo

```
.
├── .devcontainer/
│   ├── Dockerfile             ← Ubuntu 24.04 + deps verificadas
│   └── devcontainer.json      ← sin rutas hardcodeadas
├── .github/workflows/
│   └── build-kernel.yml       ← compila kernel y crea Release
├── scripts/
│   ├── 00_welcome.sh
│   ├── 01_fetch_kernel.sh     ← descarga del Release
│   ├── 02_build_kernel.sh     ← fallback: compila desde fuente
│   ├── 03_build_rootfs.sh     ← BusyBox estático + initramfs
│   └── 04_run_qemu.sh
├── Makefile
└── README.md
```

---

## Comandos disponibles

| Comando | Acción |
|---|---|
| `make setup` | Descarga kernel + arma rootfs (~5 min) |
| `make qemu` | Arranca la VM vulnerable |
| `make info` | Muestra el estado del ambiente |
| `make rootfs` | Reconstruye solo el initramfs |
| `make fetch-kernel` | Solo descarga el bzImage del Release |
| `make build-kernel` | Compila kernel desde fuente (~25 min) |
| `make clean` | Borra builds (mantiene fuentes) |
| `make clean-all` | Borra todo |

---

## Recursos del CVE

- Write-up técnico: https://xint.io/blog/copy-fail-linux-distributions
- Sitio del CVE: https://copy.fail
- PoC oficial: https://github.com/theori-io/copy-fail-CVE-2026-31431

---

## Lecciones aprendidas (referencia para futuras versiones)

Esta v2 incorpora los siguientes fixes respecto a la v1:

- `hexdump` → `bsdextrautils` en Ubuntu 24.04
- `bzip2` agregado al Dockerfile (lo necesita BusyBox)
- Eliminado el `mounts` con ruta hardcodeada en `devcontainer.json`
- Todos los scripts detectan workspace con `SCRIPT_DIR` dinámico
- Kernel: agregadas opciones críticas `BINFMT_ELF`, `BINFMT_SCRIPT`, `RD_GZIP`
- Kernel: agregada dep `CRYPTO_AEAD` antes de `CRYPTO_AUTHENCESN`
- BusyBox: reemplazado `scripts/config` (no existe) por `sed`
- BusyBox: eliminado `olddefconfig` (no existe en BusyBox)
- BusyBox: deshabilitado `CONFIG_TC` (rompe compilación con kernels nuevos)
- BusyBox: forzado `CONFIG_STATIC=y` y verificado con `file`
- Workflow Actions: greps de verificación con `|| echo`, tolerantes

Tienes toda la razón, fue un error de legibilidad al separar los bloques. Para un repositorio técnico, todo debe estar unificado en el mismo archivo para que el profesor pueda leer la historia completa de corrido, desde la teoría hasta la mitigación.

Aquí tienes el README.md completo, unificado, con el formato profesional de Markdown y listo para copiar:

## Resolución: Copy Fail Challenge B (CVE-2026-31431)

Hito 1: Reconocimiento del Entorno
Para empezar, verifiqué mi acceso a la máquina virtual vulnerable en QEMU. Ejecuté el comando uname -a para mostrar mi identificador único en el hostname y confirmar que la versión del kernel era la 6.12.0. Después, ejecuté dmesg | grep PF_ALG para comprobar que la familia de protocolos criptográficos requerida para el ataque estaba habilitada y registrada en el sistema.

Hitos 2 y 3: Análisis de Entorno y Explotación
Me encontré con un fallo de infraestructura: el entorno minimalista de BusyBox no contaba con un intérprete de Python para ejecutar el PoC. En lugar de detener el análisis, realicé un proceso de inyección de dependencias para forzar la ejecución.

Primero, modifiqué el script de construcción (03_build_rootfs.sh) para inyectar una versión completa de Python (GNU). Como esta versión requería librerías dinámicas inexistentes en BusyBox, realicé un trasplante de dependencias copiando manualmente los cargadores y librerías desde el host hacia el rootfs:

libc.so.6, libm.so.6, libdl.so.2, libpthread.so.0 (Core de glibc)

libutil.so.1, librt.so.1, libz.so.1 (Dependencias en cadena de Python)

ld-linux-x86-64.so.2 (Cargador dinámico)

Al ejecutar el exploit descubrí una limitación arquitectónica crítica: el sistema se congeló (DoS silencioso). Esto demostró que al sobrescribir /bin/su, el exploit corrompió el archivo maestro /bin/busybox en el Page Cache, ya que en BusyBox todos los comandos son symlinks al mismo inode.

Para resolverlo, compilé un binario SUID dedicado (suid_target) usando musl-gcc e inyecté el binario al rootfs como nuevo target. También corregí una fuga de file descriptors en la función c() del exploit, que agotaba el límite del kernel (EMFILE) al iterar sobre el payload sin cerrar sockets ni pipes.

Con ambas correcciones aplicadas, el resultado fue el siguiente:

Bash
~ $ /python/bin/python3 /exploit.py
[+] Page cache corrompido (1024 bytes escritos). Lanzando shell SUID...
/home/student # id
uid=0(root) gid=0(root) groups=1001(student)
Hito 4: Mitigación (Parche del Kernel)
Para mitigar la vulnerabilidad, apliqué una corrección directamente en el código fuente del kernel de Linux. Modifiqué el archivo kernel/linux/crypto/algif_aead.c. Dentro de la función _aead_recvmsg (alrededor de la línea 282), reemplacé la variable de recepción (rsgl_src) por la de transmisión (tsgl_src). Generé la evidencia de esta mitigación utilizando git diff, la cual se encuentra adjunta en el archivo hito4_mitigacion.patch en la raíz de este repositorio.

Bonus: Análisis de la Vulnerabilidad
La vulnerabilidad CVE-2026-31431 se debe a una falla lógica donde el origen y destino de una operación criptográfica AEAD compartían la misma referencia de memoria (operación in-place). Esto permitía, combinando el socket AF_ALG con la syscall splice(), forzar escrituras de datos directamente en el Page Cache de archivos de solo lectura sin permisos de escritura en disco. Al apuntar a un binario SUID, el payload se ejecutaba con privilegios de root. La solución implementada separa estrictamente los scatterlists de transmisión (TX) y recepción (RX), protegiendo la integridad del Page Cache.