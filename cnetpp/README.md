## What is cnetpp? ##

a ligthweight asynchronous network library focused on backend c++ development

## Dependancies: ##

* linux2.6 or higher, mac osx 10.10 or higher (will support more platforms in the future)
* gcc4.9.0 or higher, clang-703.0.29 or higher

## Includes: ##

* a simple json parser, named csonpp
* a simple thread framework
* the asynchronous Tcp network framework based on epoll(or select or poll)
* the asynchronous Http server and client module based on our Tcp network framework
  
## Install: ##

    cd $CNETPP_ROOT_PATH
    mkdir build/
    cd build/
    cmake -DCMAKE_INSTALL_PREFIX=/usr/local .. (modify the value of option CMAKE_INSTALL_PREFIX if you want to change the install directory)
    make
    sudo make install

