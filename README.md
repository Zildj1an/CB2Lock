# The CB2Lock

The CB2Lock: A more fair approach to handle Priority Inversion for an efficient serialization in Linux.

## Evaluation

To run the experiments n times, from x to y threads, from a to b iterations each, do as superuser:

```
./bench.sh example.config | tee -a log_file
```

or pass the environmental variables manually:

```
# TIMES=n LOW_THREAD=x HIGH_THREAD=y LOW_ITER=a HIGH_ITER=b ./bench.sh
```

## Authors

Christopher Blackburn and Carlos Bilbao.
December 2020
