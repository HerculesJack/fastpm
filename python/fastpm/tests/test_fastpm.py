from __future__ import print_function
from mpi4py_test import MPITest
from numpy.testing import assert_allclose
import numpy

import fastpm
from fastpm import PerturbationGrowth
from astropy.cosmology import FlatLambdaCDM
from pmesh.pm import ParticleMesh

import fastpm.operators as operators

@MPITest([1, 4])
def test_lpt(comm):

    from pmesh.pm import ParticleMesh

    pm = ParticleMesh(BoxSize=128.0, Nmesh=(4, 4), comm=comm)
    vm = fastpm.Evolution(pm, shift=0.5)

    dlink = pm.generate_whitenoise(1234, mode='complex')
    code = vm.code()
    code.Displace(D1=1.0, v1=0, D2=0.0, v2=0.0)
    code.Chi2(variable='s')

    power = dlink.copy()

    def kernel(k, v):
        kk = sum(ki ** 2 for ki in k)
        ka = kk ** 0.5
        p = (ka / 0.1) ** -2 * .4e4 * (1.0 / pm.BoxSize).prod()
        p[ka == 0] = 0
        return p ** 0.5 + p ** 0.5 * 1j

    power.apply(kernel, out=Ellipsis)
    dlink[...] *= power.real

    _test_model(code, dlink, power)

@MPITest([1, 4])
def test_gravity(comm):
    from pmesh.pm import ParticleMesh
    import fastpm.operators as operators

    pm = ParticleMesh(BoxSize=4.0, Nmesh=(4, 4), comm=comm, method='cic', dtype='f8')

    vm = fastpm.Evolution(pm, shift=0.5)

    dlink = pm.generate_whitenoise(12345, mode='complex')

    power = dlink.copy()

    def kernel(k, v):
        kk = sum(ki ** 2 for ki in k)
        ka = kk ** 0.5
        p = (ka / 0.1) ** -2 * .4e4 * (1.0 / pm.BoxSize).prod()
        p[ka == 0] = 0
        return p ** 0.5 + p ** 0.5 * 1j

    power.apply(kernel, out=Ellipsis)
    dlink[...] *= power.real

    code = vm.code()
    code.Displace(D1=1.0, v1=0, D2=0.0, v2=0.0)
    code.Force(factor=0.1)
    code.Chi2(variable='f')

    # FIXME: without the shift some particles have near zero dx1.
    # or near 1 dx1.
    # the gradient is not well approximated by the numerical if
    # any of the left or right value shifts beyond the support of
    # the window.
    #
    _test_model(code, dlink, power)

def _test_model(code, dlin_k, ampl):

    def objective(dlin_k):
        init = {'dlin_k': dlin_k}
        return code.compute('chi2', init, monitor=None)

    def gradient(dlin_k):
        tape = code.vm.tape()
        init = {'dlin_k': dlin_k}
        code.compute(['chi2'], init, tape=tape, monitor=None)
        gcode = code.vm.gradient(tape)
        init = {'_chi2' : 1}
        return gcode.compute('_dlin_k', init, monitor=None)

    y0 = objective(dlin_k)
    yprime = gradient(dlin_k)

    num = []
    ana = []
    print('------')
    for ind1 in numpy.ndindex(*(list(dlin_k.cshape) + [2])):
        dlinkl = dlin_k.copy()
        dlinkr = dlin_k.copy()
        old = dlin_k.cgetitem(ind1)
        pert = ampl.cgetitem(ind1) * 1e-5
        left = dlinkl.csetitem(ind1, old - pert)
        right = dlinkr.csetitem(ind1, old + pert)
        diff = right - left
        yl = objective(dlinkl)
        yr = objective(dlinkr)
        grad = yprime.cgetitem(ind1)
        print(ind1, old, pert, yl, yr, grad * diff, yr - yl)
        ana.append(grad * diff)
        num.append(yr - yl)
    print('------')

    assert_allclose(num, ana, rtol=1e-3)

@MPITest([1, 4])
def test_kdk(comm):
    pt = PerturbationGrowth(FlatLambdaCDM(Om0=0.3, H0=70, Tcmb0=0))

    pm = ParticleMesh(BoxSize=128.0, Nmesh=(4,4,4), comm=comm, dtype='f8')
    vm = fastpm.KickDriftKick(pm, shift=0.5)
    dlink = pm.generate_whitenoise(12345, mode='complex', unitary=True)
    power = dlink.copy()

    def kernel(k, v):
        kk = sum(ki ** 2 for ki in k)
        ka = kk ** 0.5
        p = (ka / 0.1) ** -2 * .4e4 * (1.0 / pm.BoxSize).prod()
        p[ka == 0] = 0
        return p ** 0.5 + p ** 0.5 * 1j

    power.apply(kernel, out=Ellipsis)
    dlink[...] *= power.real

    data = dlink.c2r()
    data[...] = 0
    sigma = data.copy()
    sigma[...] = 1.0

    code = vm.simulation(pt, 0.1, 1.0, 5)
    code.Paint()
    code.Diff(data_x=data, sigma_x=sigma)
    code.Chi2(variable='mesh')

    _test_model(code, dlink, power)
