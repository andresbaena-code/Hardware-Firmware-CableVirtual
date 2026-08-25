"""Verifica el criterio SC-008 y el Principio VII: trazabilidad requisito -> codigo.

Cada requisito funcional de control declarado en la especificacion debe aparecer
citado en al menos un archivo del codigo fuente o de la infraestructura del
proyecto. Sin esta comprobacion, las anotaciones FR-xxx se degradan en silencio a
medida que el codigo evoluciona, y la trazabilidad se vuelve una promesa en vez de
un hecho.

Los requisitos de proyecto (build reproducible, pruebas, documentacion) se
satisfacen citandolos en la infraestructura: platformio.ini, los workflows o la
documentacion. No tienen un modulo de C++ que los implemente.

Uso:  python check_traceability.py [raiz_del_repositorio]
"""
import pathlib
import re
import sys

# Requisitos de proyecto: se verifican en infraestructura, no en modulos de C++.
REQUISITOS_DE_PROYECTO = {
    "FR-030",  # build reproducible desde VS Code
    "FR-031",  # la logica se verifica sin hardware
    "FR-032",  # cada regla de control tiene prueba
    "FR-033",  # sketches originales conservados
    "FR-034",  # documentacion de puesta en marcha
    "NFR-001",  # presupuesto de RAM
    "NFR-002",  # operacion prolongada
    "NFR-003",  # comportamiento electrico no empeora
}

EXTENSIONES_CODIGO = {".h", ".cpp", ".ini"}
EXTENSIONES_INFRA = {".yml", ".yaml", ".py", ".md"}


def recolectar(raiz: pathlib.Path, extensiones: set, subdirs) -> set:
    encontrados = set()
    for sub in subdirs:
        base = raiz / sub
        if not base.exists():
            continue
        for f in base.rglob("*"):
            if not f.is_file() or f.suffix not in extensiones:
                continue
            if ".pio" in f.parts or "node_modules" in f.parts:
                continue
            texto = f.read_text(encoding="utf-8", errors="replace")
            encontrados |= set(re.findall(r"\b(?:FR|NFR)-\d+b?\b", texto))
    return encontrados


def main() -> int:
    raiz = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else ".").resolve()
    spec = raiz / "specs" / "001-migracion-platformio" / "spec.md"

    if not spec.is_file():
        print(f"::error::No se encontro la especificacion en {spec}")
        return 1

    declarados = re.findall(r"\*\*((?:FR|NFR)-\d+b?)\*\*:", spec.read_text(encoding="utf-8"))
    if not declarados:
        print("::error::La especificacion no declara ningun requisito.")
        return 1

    en_codigo = recolectar(raiz, EXTENSIONES_CODIGO, ["CABLE_VIRTUAL_V2_PIO"])
    en_infra = recolectar(raiz, EXTENSIONES_INFRA, [".github", "docs"])

    faltan = []
    for req in declarados:
        if req in REQUISITOS_DE_PROYECTO:
            if req not in (en_codigo | en_infra):
                faltan.append((req, "requisito de proyecto sin citar en infraestructura ni documentacion"))
        elif req not in en_codigo:
            faltan.append((req, "requisito de control sin citar en ningun modulo de codigo"))

    total = len(declarados)
    cubiertos = total - len(faltan)
    print(f"Trazabilidad: {cubiertos}/{total} requisitos citados "
          f"({len(en_codigo)} en codigo, {len(en_infra)} en infraestructura).")

    if faltan:
        print()
        for req, motivo in faltan:
            print(f"::error::{req}: {motivo}")
        print()
        print("El criterio SC-008 y el Principio VII exigen que todo requisito sea")
        print("rastreable hasta el modulo que lo implementa. Anadir el identificador")
        print("al comentario de cabecera del modulo correspondiente.")
        return 1

    return 0


if __name__ == "__main__":
    sys.exit(main())
