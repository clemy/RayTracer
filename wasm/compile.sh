mkdir -p out
cp html_template/styles.css out/
cp html_template/bunny.png out/
emcc -O3 -s WASM=1 -std=c++17 -pthread -lz -s USE_ZLIB=1 ../src/*.cpp -o out/raytracer.html --preload-file examples -s ASSERTIONS=1 -s ALLOW_MEMORY_GROWTH=1 -s PROXY_TO_PTHREAD=1 -s EXIT_RUNTIME=0 --shell-file html_template/shell_minimal.html -s DISABLE_EXCEPTION_CATCHING=0 -s EXPORTED_RUNTIME_METHODS='["callMain"]'
