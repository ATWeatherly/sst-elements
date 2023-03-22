# Balar devnote

Some notes on balar development

## Usage

After successful compilation and installation of SST core and SST elements, run:

```bash
cd tests/
# Run with testCPU
sst testBalar-testcpu.py --model-options='-c ariel-gpu-v100.cfg -v -x vectorAdd/vectorAdd'
# Run with vanadis core
sst testBalar-vanadis.py --model-options='-c ariel-gpu-v100.cfg'
```

Can also set `PTX_SIM_DEBUG=3` to enable debug output in GPGPU-Sim, check its manual for more details.

## TODO

1. [x] Link the custom cuda lib with nvcc executable
2. [x] Create an interface in Balar using MMIO to accept arguments and send results
    1. [x] Test if can receive and send data properly with standardCPU
    1. Need to source the gpgpusim and cuda script
        1. `source ../env-setup/sst_env_setup.sh`
        1. `source ../sst-gpgpusim-external/setup_environment`
    1. Need to make and install the sst
        1. `make -j`
        1. `make install`
    1. Run script: `sst testBalar-simple.py --model-options="-c ariel-gpu-v100.cfg -v" > tmp.out 2>&1`
3. [ ] Real test
    1. [x] How to generate test stimulus?
    2. [x] Actually pass arguments to gpgpusim
    3. [x] VectorAdd example?
    4. [x] Get cuda calls arguments from old balar
        1. [ ] Automated dump cuda call sequences and generate launch/config file to the testcpu to consume
        2. [x] Or NVBit CUDA calls?
4. [ ] Like a unit-test feel use StandardCPU to pass stimulus (CUDA arguments) to balar
5. [ ] Pass PTX file directly? 
6. [ ] What about timing simulation for cuda calls? Earlier one has gpu caches and all sort of thing
7. [ ] SST Cuda call callbacks in `balarMMIO.cc` to simulate cache latency in SST?

## Issues

1. [ ] Need to wait for kernel launch to be completed before issuing D2H copies
