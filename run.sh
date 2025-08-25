cmake -B build/ -S . -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_FLAGS="-fsanitize=address"
cmake --build ./build --config Debug
sudo ./build/http-server
