from spack import *

class Lcw(CMakePackage):
    """LCW: a Lightweight Communication Wrapper"""

    homepage = "https://github.com/JiakunYan/lcw.git"
    git      = "https://github.com/JiakunYan/lcw.git"

    maintainers("JiakunYan")

    version('master', branch='master')

    def is_positive_int(val):
        try:
            return int(val) > 0
        except ValueError:
            return val == 'auto'

    variant('backend', default='mpi,lci,lci2', values=('mpi', 'lci', "lci2"), multi=True,
            description='Communication backend')
    variant('shared', default=True,  description='Build with shared libraries')
    variant('examples', default=True, description='Build LCW examples')
    variant('cache-line', default='auto', values=is_positive_int,
            description='Cache line size, in bytes')
    variant('debug', default=False, description='Enable debug mode')
    variant('pcounter', default=False,
            description='Use performance counter')

    depends_on("mpi", when="backend=mpi")
    depends_on("lci")
    depends_on("c", type="build")
    depends_on("cxx", type="build")

    def cmake_args(self):
        args = [
            self.define_from_variant('BUILD_SHARED_LIBS', 'shared'),
            self.define_from_variant('LCW_WITH_EXAMPLES', 'examples'),
            self.define_from_variant('LCW_DEBUG', 'debug'),
            self.define_from_variant('LCW_ENABLE_PCOUNTER', 'pcounter'),
        ]

        if self.spec.satisfies("backend=mpi"):
            arg = self.define('LCW_TRY_ENABLE_BACKEND_MPI', True)
            args.append(arg)
        else:
            arg = self.define('LCW_TRY_ENABLE_BACKEND_MPI', False)
            args.append(arg)

        if self.spec.satisfies("backend=lci"):
            arg = self.define('LCW_TRY_ENABLE_BACKEND_LCI', True)
            args.append(arg)
        else:
            arg = self.define('LCW_TRY_ENABLE_BACKEND_LCI', False)
            args.append(arg)

        if self.spec.satisfies("backend=lci2"):
            arg = self.define('LCW_TRY_ENABLE_BACKEND_LCI2', True)
            args.append(arg)
        else:
            arg = self.define('LCW_TRY_ENABLE_BACKEND_LCI2', False)
            args.append(arg)

        if self.spec.variants['cache-line'].value != 'auto':
            arg = self.define_from_variant('LCW_CACHE_LINE', 'cache-line')
            args.append(arg)

        return args
