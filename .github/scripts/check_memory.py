"""Verifica el presupuesto de memoria del firmware contra el limite de NFR-001.

Lee el reporte que PlatformIO imprime al compilar y falla si la RAM ocupada supera
el porcentaje maximo permitido. El margen restante es la pila: agotarlo produce
corrupcion silenciosa en tiempo de ejecucion, no un error de compilacion.

Uso:  python check_memory.py <build.log> [limite_ram_pct]
"""
import os
import pathlib
import re
import sys

PATRON_RAM = r"RAM:\s+\[[^\]]*\]\s+([\d.]+)%\s+\(used (\d+) bytes from (\d+) bytes\)"
PATRON_FLASH = r"Flash:\s+\[[^\]]*\]\s+([\d.]+)%\s+\(used (\d+) bytes from (\d+) bytes\)"


def main() -> int:
    if len(sys.argv) < 2:
        print("uso: check_memory.py <build.log> [limite_ram_pct]", file=sys.stderr)
        return 2

    log_path = pathlib.Path(sys.argv[1])
    limite = float(sys.argv[2]) if len(sys.argv) > 2 else 75.0

    if not log_path.is_file():
        print(f"::error::No existe el registro de compilacion: {log_path}")
        return 1

    log = log_path.read_text(errors="replace")
    ram = re.search(PATRON_RAM, log)
    flash = re.search(PATRON_FLASH, log)

    if not ram or not flash:
        print("::error::No se pudo leer el reporte de memoria del build.")
        return 1

    ram_pct = float(ram.group(1))
    flash_pct = float(flash.group(1))

    print(
        f"RAM {ram_pct}% ({ram.group(2)}/{ram.group(3)} B) · "
        f"Flash {flash_pct}% ({flash.group(2)}/{flash.group(3)} B)"
    )

    resumen = os.environ.get("GITHUB_STEP_SUMMARY")
    if resumen:
        with open(resumen, "a", encoding="utf-8") as f:
            f.write("### Uso de memoria — Arduino Mega 2560\n\n")
            f.write("| Recurso | Usado | Total | Porcentaje | Límite |\n")
            f.write("|---|---|---|---|---|\n")
            f.write(
                f"| RAM | {ram.group(2)} B | {ram.group(3)} B | "
                f"**{ram_pct} %** | {limite} % |\n"
            )
            f.write(
                f"| Flash | {flash.group(2)} B | {flash.group(3)} B | {flash_pct} % | — |\n"
            )

    if ram_pct > limite:
        print(
            f"::error::La RAM ocupada ({ram_pct} %) supera el limite de {limite} % "
            f"que fija NFR-001. El margen restante es la pila."
        )
        return 1

    return 0


if __name__ == "__main__":
    sys.exit(main())
