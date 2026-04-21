make -C ../_my/zlib clean
make -C ../_my/zlib
rm -f ./lib/libz_malicious.a
rm -f ./lib/libz_evil.so
cp ../_my/zlib/libz.a ./lib/libz_malicious.a
