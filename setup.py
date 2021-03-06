from skbuild import setup

setup(
    name="fenics-libtab",
    version="0.0.1",
    description="FEniCS tabulation library",
    author="FEniCS Project",
    license="MIT",
    packages=["libtab"],
    package_dir={"": "cpp"},
    cmake_install_dir="cpp/libtab",
)
