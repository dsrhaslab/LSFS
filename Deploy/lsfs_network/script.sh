step=`bc -l <<< 'scale=6; 1/100'`;res=`seq 0 ${step} 1`;exit $res
