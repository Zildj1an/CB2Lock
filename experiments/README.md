The Slowdown of a thread T under certain Priority Inversion mechanism PI\_method
where the HP and LP threads acquire the lock once each is computed as:

Slowdown\_T(PI\_method)= CPU\_percentage\_T(PI\_method) / CPU\_percentage\_T(normal) 

where "normal" defines a typical scenario in which all threads have the same priority and
we used the default mutex lock. The Iterations under such conditions are extracted from
an average of a high number of executions.

