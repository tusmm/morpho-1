# Morpho
The Morpho language. Morpho is a programmable environment for shape optimization. Morpho aims to be:

* **Familiar**. Morpho uses syntax similar to other C-family languages. The syntax fits on a postcard, so it's easy to learn.
* **Fast**. Morpho programs run as efficiently as other well-implemented dynamic languages like *wren* or *lua* (Morpho is significantly faster than Python, for example). Morpho leverages numerical libraries like BLAS, LAPACK and SUITE-SPARSE to provide high performance.
* **Class-based**. A morpho program involves creating and manipulating objects, which greatly simplifies operation.
* **Extendable**. Morpho is, in effect, an embeddable language oriented for scientific applications. Functionality is easy to add via packages.

## Installation

Morpho can be installed as follows:

### macOS

1. Install the [Homebrew](https://brew.sh) package manager, following instructions on the homebrew site.

2. Install dependencies. Open the Terminal application and type:

    brew update

    brew install glfw suite-sparse

3. Obtain the source by cloning this repository:

    git clone https://github.com/Morpho-lang/morpho.git

4. Navigate to the `morpho5` folder and build the application:

    cd morpho/morpho5

    make install

5. Navigate to the `morpho5` folder and build the viewer application:

    cd ../morphoview

    make install

6. Check that the application works by typing

    morpho5

### Unix and Linux

2. Install morpho's dependencies using your distribution's package manager (or manually if you prefer). For example, on Ubuntu you would type

    apt-get glfw

    apt-get suite-sparse

3. Obtain the source by cloning this repository:

    git clone https://github.com/Morpho-lang/morpho.git

4. Navigate to the `morpho5` folder and build the application:

    cd morpho/morpho5

    sudo make -f Makefile.linux install

5. Navigate to the `morphoview` folder and build the viewer application:

    cd ../morphoview
    
    sudo make -f Makefile.linux install

6. Check that the application works by typing

    morpho5

Note that the build script places morpho5 and morphoview in `/usr/local/`; this can easily be changed if a different location is preferred.
