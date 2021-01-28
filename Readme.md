cmark-gfm
Estado de construcción Estado de la compilación de Windows

cmark-gfmes una versión extendida de la implementación de referencia C de CommonMark , una versión racionalizada de la sintaxis de Markdown con una especificación. Este repositorio agrega extensiones de GitHub Flavored Markdown a la implementación ascendente , como se define en la especificación .

El resto del archivo README se conserva tal cual desde la fuente ascendente. Tenga en cuenta que la biblioteca y los binarios producidos por esta bifurcación tienen el sufijo -gfmpara distinguirlos del flujo ascendente.

Proporciona una biblioteca compartida ( libcmark) con funciones para analizar documentos CommonMark en un árbol de sintaxis abstracta (AST), manipular el AST y representar el documento en HTML, groff man, LaTeX, CommonMark o una representación XML del AST. También proporciona un programa de línea de comandos ( cmark) para analizar y representar documentos CommonMark.

Ventajas de esta biblioteca:

Portátil. La biblioteca y el programa están escritos en C99 estándar y no tienen dependencias externas. Se han probado con MSVC, gcc, tcc y clang.

Rápido. cmark puede generar una versión Markdown de War and Peace en un abrir y cerrar de ojos (127 milisegundos en una computadora portátil de diez años, frente a 100-400 milisegundos en un abrir y cerrar de ojos). En nuestros puntos de referencia , cmark es 10.000 veces más rápido que el original Markdown.ply está a la par con los procesadores Markdown más rápidos disponibles.

Preciso. La biblioteca pasa todas las pruebas de conformidad de CommonMark.

Estandarizado. Se puede esperar que la biblioteca analice CommonMark de la misma forma que cualquier otro analizador conforme. Entonces, por ejemplo, puede usar commonmark.jsen el cliente para obtener una vista previa del contenido que se procesará en el servidor usando cmark.

Robusto. La biblioteca ha sido extensamente probada usando fuzzy lop americano . El conjunto de pruebas incluye casos patológicos que arrastran a muchos otros analizadores de Markdown (por ejemplo, texto entre corchetes anidado de miles de profundidad o comillas en bloque).

Flexible. La entrada de CommonMark se analiza en un AST que se puede manipular mediante programación antes de renderizar.

Varios renderizadores. Se admite la salida en HTML, groff man, LaTeX, CommonMark y un formato XML personalizado. Y es fácil escribir nuevos renderizadores para admitir otros formatos.

Gratis. Con licencia BSD2.

Es fácil de usar libcmarken python, lua, ruby ​​y otros lenguajes dinámicos: consulte el wrappers/subdirectorio para ver algunos ejemplos simples.

Hay también bibliotecas que se envuelven libcmarkpara Go , Haskell , Rubí , Lua , Perl , Python , R , Tcl , Scala y Node.js .

Instalando
La construcción del programa C ( cmark) y la biblioteca compartida ( libcmark) requiere cmake . Si modifica scanners.re, también necesitará re2c (> = 0.14.2), que se usa para generar scanners.cdesde scanners.re. Hemos incluido un pregenerado scanners.cen el repositorio para reducir las dependencias de compilación.

Si tiene GNU make, puede simplemente make, make testy make install. Esto llama a cmake para crear un Makefileen el build directorio, luego lo usa Makefilepara crear el ejecutable y la biblioteca. Los binarios se pueden encontrar en build/src. El prefijo de instalación predeterminado es /usr/local. Para cambiar el prefijo de instalación, pase la INSTALL_PREFIXvariable si se ejecuta makepor primera vez: make INSTALL_PREFIX=path.

Para un método más portátil, puede usar cmake manualmente. cmake sabe cómo crear entornos de construcción para muchos sistemas de construcción. Por ejemplo, en FreeBSD:

mkdir build
cd build
cmake ..  # optionally: -DCMAKE_INSTALL_PREFIX=path
make      # executable will be created as build/src/cmark
make test
make install
O, para crear archivos de proyecto Xcode en OSX:

mkdir build
cd build
cmake -G Xcode ..
open cmark.xcodeproj
GNU Makefile también proporciona algunos otros objetivos para los desarrolladores. Para ejecutar un punto de referencia:

make bench
Para obtener evaluaciones comparativas más detalladas:

make newbench
Para ejecutar una prueba de fugas de memoria usando valgrind:

make leakcheck
Para reformatear el código fuente usando clang-format:

make format
Para ejecutar una "prueba de fuzz" contra diez entradas largas generadas aleatoriamente:

make fuzztest
Para hacer una prueba de fuzz más sistemática con american fuzzy lop :

AFL_PATH=/path/to/afl_directory make afl
Fuzzing con libFuzzer también es compatible pero, debido a que libFuzzer aún está en desarrollo activo, es posible que no funcione con la versión de clang instalada en el sistema. Suponiendo que LLVM se ha construido en $HOME/src/llvm/buildel fuzzer, se puede ejecutar con:

CC="$HOME/src/llvm/build/bin/clang" LIB_FUZZER_PATH="$HOME/src/llvm/lib/Fuzzer/libFuzzer.a" make libFuzzer
Para crear un archivo tarball y zip de lanzamiento:

make archive
Instalación (Windows)
Para compilar con MSVC y NMAKE:

nmake
Puede realizar una compilación cruzada de un binario de Windows y un dll en linux si tiene el mingw32compilador:

make mingw
Los binarios estarán en formato build-mingw/windows/bin.

Uso
Las instrucciones para el uso del programa de línea de comandos y la biblioteca se pueden encontrar en las páginas de manual del mansubdirectorio.

Seguridad
Por defecto, la biblioteca fregar HTML puro y enlaces potencialmente peligrosos ( javascript:, vbscript:, data:, file:).

Para permitirlos, use la opción CMARK_OPT_UNSAFE(o --unsafe) con el programa de línea de comando. Si lo hace, le recomendamos que utilice un desinfectante HTML específico para sus necesidades para protegerse contra los ataques XSS .

Contribuyendo
Hay un foro para discutir CommonMark ; debe usarlo en lugar de problemas de github para preguntas y posiblemente discusiones abiertas. Utilice el rastreador de problemas de github solo para problemas simples, claros y procesables.

Autores
John MacFarlane escribió la biblioteca y el programa originales. El algoritmo de análisis de bloques se elaboró ​​junto con David Greenspan. Vicent Marti optimizó la implementación de C para el rendimiento, multiplicando por diez su velocidad. Kārlis Gaņģis ayudó a desarrollar un mejor algoritmo de análisis de enlaces y énfasis, eliminando varios problemas de rendimiento del peor de los casos. Nick Wellnhofer contribuyó con muchas mejoras, incluida la mayor parte de la API de la biblioteca C y su mazo de pruebas
