sudo sed -i 's+{{mirror}}+http://us-central1.gce.archive.ubuntu.com/ubuntu/+g;s+{{codename}}+focal+g;s+{{security}}+http://security.ubuntu.com/ubuntu+g' /etc/apt/sources.list
sudo apt update
sudo apt-get install -y dstat && sudo apt install -y screen 

cd metrics
nvidia-smi --query-gpu=timestamp,utilization.gpu,utilization.memory,memory.total,memory.free,memory.used --format=csv -l 1 -f nvidia-smi-local-1.csv &
cd ..

bash dstat/init_dstat.sh home/brancojse/metrics dstat-local-1 

bash train.sh

bash dstat/stop_dstat.sh
pgrep -f nvidia-smi
kill 




main_confs:
  use_localhost: False
  peer:
    log_level: off
    mt_data_handler: true
    warmup_interval: 120
    restart_database_after_warmup: true
    reply_chance: 1
    smart_message_forward: true
    seen_log_garbage_at: 1000
    request_log_garbage_at: 1000
    anti_entropy_log_garbage_at: 1000
    database:
      base_path: /shared_dir/
    anti_entropy:
      dissemination_interval_sec: 60
      dissemination_total_packet_size_percentage: 10 # 4k = 7%-6%
      max_keys_to_send_percentage: 5

  group_construction:
    rep_max: 35
    rep_min: 25
    max_age: 30

  pss:
    view_size: 20
    gossip_size: 9
    message_passing_interval_sec: 15
    local_message: true
    local_interval_sec: 10

  log:
    view_logger_enabled: false
    log_interval_sec: 5
    logging_dir: logging/

  client:
    log_level: off
    base_path: /shared_dir/
    max_nodes_to_send_get_request: 3
    max_nodes_to_send_put_request: 1
    nr_puts_required: 1
    nr_gets_required: 1
    nr_gets_version_required: 1
    client_wait_timeout: 5
    max_nr_requests_timeouts: 100
    limit_write_paralelization_to: 64k
    limit_read_paralelization_to: 32k
    mt_client_handler: true
    nr_client_handler_ths: 4
    load_balancer:
      type: smart
      lb_interval: 5
      smart_load_balancer_group_knowledge: 30