## Amalgamation demo

While in the idna main directory, using Python 3, type:

```
python singleheader/amalgamate.py
```

This will create two new files (ada_idna.h and ada_idna.cpp).

You can then compile the demo file as follows:

```
c++ -std=c++17 -c demo.cpp
```

It will produce a binary file (e.g., demo.o) which contains ada_idna.cpp.
