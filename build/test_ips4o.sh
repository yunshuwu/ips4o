#!/bin/bash
make
export CILK_NWORKERS=192
NUM_ELEMENTS=1000000000

# for uniform distribution
uniform_max=(10 100 1000 5000 7000 8000 10000 15000 20000 50000 100000 1000000 10000000 100000000 1000000000)
for ((i=0;i<${#uniform_max[@]};i++))
do
    echo "Uniform Distribution ${i}" >> test_ips4o_1e9
    echo "Key range  = (0, ${uniform_max[i]})" >> test_ips4o_1e9
    nohup numactl -i all ./ips4o_example ${NUM_ELEMENTS} uniform ${uniform_max[i]} >> test_ips4o_1e9
done

# # for exponential distribution
# exp_lambda=(1000 10000 100000 1000000 10000000 100000000)
# for  ((i=0;i<${#exp_lambda[@]};i++))
# do
#     echo "Exponential Distribution ${i}" >> test_ips4o_1e9
#     echo "Exponential lambda = ${exp_lambda[i]}" >> test_ips4o_1e9
#     nohup numactl -i all ./ips4o_example ${NUM_ELEMENTS} exponential ${exp_lambda[i]} >> test_ips4o_1e9
# done

# for zipfian distribution
zipfian_s=(10000 100000 1000000 10000000 100000000 1000000000)
for  ((i=0;i<${#zipfian_s[@]};i++))
do
    echo "Zipfian Distribution ${i}" >> test_ips4o_1e9
    echo "Zipfian s = ${zipfian_s[i]}" >> test_ips4o_1e9
    nohup numactl -i all ./ips4o_example ${NUM_ELEMENTS} zipfian ${zipfian_s[i]} >> test_ips4o_1e9
done
