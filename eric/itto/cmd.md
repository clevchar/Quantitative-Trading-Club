```
clang -O3 -march=native -ffast-math -funroll-loops deciphering2.c -o deciphering2
```

squeezes a bit more with flto:
```
clang -O3 -march=native -ffast-math -flto -funroll-loops deciphering2.c -o deciphering2
```