"""Compile the actual Kotlin JNI owner using cached dependencies, without AGP or downloads."""
from pathlib import Path
import shutil
import subprocess
import sys

jdk, output = map(Path, sys.argv[1:])
cache = Path.home() / ".gradle/caches/modules-2/files-2.1"
test_dir = Path(__file__).resolve().parent
source = test_dir.parent.parent / "main/java/com/example/oboesample/AudioEngine.kt"


def cached(group, module, version):
    filename = f"{module}-{version}.jar"
    matches = list((cache / group / module / version).glob(f"*/{filename}"))
    if not matches:
        raise SystemExit(f"Missing cached dependency: {group}:{module}:{version}")
    return matches[0]


stdlib = cached("org.jetbrains.kotlin", "kotlin-stdlib", "2.0.21")
annotation = cached("androidx.annotation", "annotation-jvm", "1.8.1")
compiler = [
    cached("org.jetbrains.kotlin", "kotlin-compiler-embeddable", "2.0.21"),
    stdlib,
    cached("org.jetbrains.kotlin", "kotlin-script-runtime", "2.0.21"),
    cached("org.jetbrains.kotlin", "kotlin-reflect", "2.0.21"),
    cached("org.jetbrains.intellij.deps", "trove4j", "1.0.20200330"),
    cached("org.jetbrains.kotlinx", "kotlinx-coroutines-core-jvm", "1.6.4"),
    cached("org.jetbrains", "annotations", "13.0"),
]
classpath = ":".join(map(str, [stdlib, annotation]))
classes = output / "classes"
classes.mkdir()
subprocess.run([
    str(jdk / "bin/java"), "-cp", ":".join(map(str, compiler)),
    "org.jetbrains.kotlin.cli.jvm.K2JVMCompiler", "-no-stdlib", "-no-reflect",
    "-jvm-target", "21", "-classpath", classpath, "-d", str(classes), str(source),
], check=True)
subprocess.run([
    str(jdk / "bin/javac"), "-classpath", f"{classes}:{classpath}", "-d", str(classes),
    str(test_dir / "AudioEngineLifecycleTest.java"),
], check=True)
shutil.copyfile(stdlib, output / "kotlin-stdlib.jar")
shutil.copyfile(annotation, output / "annotation.jar")
