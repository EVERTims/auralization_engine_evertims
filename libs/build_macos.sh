#!/bin/sh

# ----------------
## init
# ----------------

# ensure current dir is .
LIB_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd $LIB_DIR

# fetch submodules
git submodule update --init --recursive

# check for homebrew install
which -s brew
if [[ $? != 0 ]] ; then
    # Suggest to install Homebrew
    echo 'script aborted: install homebrew to continue (check https://brew.sh/index_fr.html)'
    exit
    # /usr/bin/ruby -e "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/master/install)"
else
    brew update
fi

# ----------------
## build libmysofa
# ----------------

# install deps if need be
for pkg in libresample cunit; do
    if brew list -1 | grep -q "^${pkg}\$"; then
        echo "Package '$pkg' already installed"
    else
        echo "Installing Package '$pkg'"
        brew install ${pkg}
    fi
done

# compile
cd libmysofa/build/
cmake ..
make && make install
