sudo apt-get install -y dstat && apt install -y screen 

cd metrics
nvidia-smi --query-gpu=timestamp,utilization.gpu,utilization.memory,memory.total,memory.free,memory.used --format=csv -l 1 -f nvidia-smi-local-1.csv &
cd ..

bash dstat/init_dstat.sh home/brancojse/metrics dstat-local-1 

bash train.sh

bash dstat/stop_dstat.sh
pgrep -f nvidia-smi
kill 
