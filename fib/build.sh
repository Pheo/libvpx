emcc fibonacci.c -s ENVIRONMENT=web -s MODULARIZE=1 -s EXPORT_ES6=1 -s USE_ES6_IMPORT_META=0 -s EXPORTED_FUNCTIONS=['_fibonacci','_main']
