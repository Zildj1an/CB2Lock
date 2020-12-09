# The CB2Lock

The CB2Lock: A more fair approach to handle Priority Inversion for an efficient serialization in Linux.

## Evaluation

To run the experiments n times from x to y threads do as superuser:

```
# ITERATIONS=n LOW_THREAD=x HIGH_THREAD=y ./bench.sh | tee -a log_file
```

## Authors

Christopher Blackburn and Carlos Bilbao.
December 2020
