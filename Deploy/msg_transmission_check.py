import regex as re
import os
from collections import defaultdict
import dateutil.parser
import datetime
import pickle
from statistics import mean

folder = "logging"
msg_data = None
analised_data = None
last_min_time_sent = None
max_time_to_rcv_msg = 1
from_pickle = True
client_ip = "10.244.1.90"

def default_to_regular(d):
    if isinstance(d, defaultdict):
        d = {k: default_to_regular(v) for k, v in d.items()}
    return d

def save_to_file():
    global msg_data

    file_pi = open('msg_data.dat', 'wb+')
    pickle.dump(msg_data, file_pi)

def load_from_file():
    global msg_data, last_min_time_sent
    datetime_str = "2020-06-29 22:56:14"
    last_min_time_sent = datetime.datetime.strptime(datetime_str, '%Y-%m-%d %H:%M:%S')

    file_pi2 = open('msg_data.dat', 'rb')
    msg_data = pickle.load(file_pi2)

def read_files():
    global folder, msg_data, last_min_time_sent

    temp_data = defaultdict(lambda: defaultdict(lambda: defaultdict(lambda: defaultdict(lambda: defaultdict(int)))))

    for file in os.listdir(folder):
        print(file)
        filepath = os.path.join(folder, file)
        with open(filepath) as f:
            ip_last_time_sent = None

            for line in f:
                m = re.match(r'\[([^\]]+)\] (\w+) ([\d\.]+)([\-\>\<]+)([\d\.]+)', line)
                if m:
                    time = dateutil.parser.parse(m[1])
                    msg_type = m[2]
                    orientation = m[4]
                    if(orientation == "->"):
                        sender_ip = m[3]
                        target_ip = m[5]
                        temp_data[sender_ip]['sent'][msg_type][target_ip][time] += 1
                        ip_last_time_sent = time
                    else:
                        receiver_ip = m[3]
                        source_ip = m[5]
                        temp_data[receiver_ip]['recv'][msg_type][source_ip][time] += 1

            if not last_min_time_sent:
                last_min_time_sent = ip_last_time_sent
            elif ip_last_time_sent < last_min_time_sent:
                print(ip_last_time_sent)
                print("<")
                print(last_min_time_sent)
                last_min_time_sent = ip_last_time_sent

    return default_to_regular(temp_data)

def remove_messages_rcv_after_time(ip, msg_type, target_ip, hora, count):
    global msg_data, max_time_to_rcv_msg

    nr_msgs_rcv = 0

    if target_ip not in msg_data:
        return 0
    elif 'recv' not in msg_data[target_ip]:
        return 0
    elif msg_type not in msg_data[target_ip]['recv']:
        return 0
    elif ip not in msg_data[target_ip]['recv'][msg_type]:
        return 0

    messages_rcv = [(hora_rcv, count_rcv) for hora_rcv, count_rcv in msg_data[target_ip]['recv'][msg_type][ip].items()]
    messages_rcv.sort(key=lambda x: x[0])

    for hora_rcv, count_rcv in messages_rcv:
        if(hora_rcv < hora):
            continue
        if count > 0 and hora_rcv >= hora and hora_rcv <= (hora + datetime.timedelta(0, max_time_to_rcv_msg)):
            if count_rcv > count:
                nr_msgs_rcv += count
                msg_data[target_ip]['recv'][msg_type][ip][hora_rcv] -= count
                count = 0
                break
            else:
                nr_msgs_rcv += count_rcv
                count -= count_rcv
                del msg_data[target_ip]['recv'][msg_type][ip][hora_rcv]

    return nr_msgs_rcv


def analise_data():
    global msg_data, analised_data, last_min_time_sent, client_ip

    analised_data = defaultdict(lambda: defaultdict(int))
    for ip in msg_data:
        for msg_type in msg_data[ip]['sent']:
            for target_ip in msg_data[ip]['sent'][msg_type]:
                for hora, count in msg_data[ip]['sent'][msg_type][target_ip].items():
                    if(hora <= last_min_time_sent):
                        nr_messages_rcv = remove_messages_rcv_after_time(ip, msg_type, target_ip, hora, count)
                        analised_data[ip]['sent_and_rcv'] += nr_messages_rcv
                        analised_data[ip]['sent_not_rcv'] += (count - nr_messages_rcv)
                        msg_data[ip]['sent'][msg_type][target_ip][hora] -= nr_messages_rcv
                        if target_ip == client_ip:
                            analised_data[ip]['sent_not_rcv_from_client'] += (count - nr_messages_rcv)
                        if(count > nr_messages_rcv):
                            print(hora)
                            print(msg_type + " " + ip + "->" + target_ip + "    " + str(count) + ":" + str(nr_messages_rcv))

if from_pickle:
    load_from_file()
    # horinha = datetime.datetime.strptime('2020-06-29 22:56:00', '%Y-%m-%d %H:%M:%S')
    # print("--------------------------------------------------------")
    # print(msg_data['10.244.1.89']['sent']['P']['10.244.1.84'][horinha])
    # print(msg_data['10.244.1.84']['recv']['P']['10.244.1.89'][horinha])
else:
    msg_data = read_files()
    save_to_file()
    
analise_data()
print(analised_data)
print("sent not recv: " + str(sum([ analised_data[ip]['sent_not_rcv'] for ip in analised_data])))
print("sent and recv: " + str(sum([ analised_data[ip]['sent_and_rcv'] for ip in analised_data])))
sent_not_rcv = [ analised_data[ip]["sent_not_rcv"] for ip in analised_data if ip != client_ip]
print("sent not rcv: (" + str(mean(sent_not_rcv)) + ")")
print(sent_not_rcv)
sent_not_rcv_from_client = [ analised_data[ip]["sent_not_rcv_from_client"] for ip in analised_data if ip != client_ip]
print("sent not rcv from client: (" + str(mean(sent_not_rcv_from_client)) + ")")
print(sent_not_rcv_from_client)
print("client sent and recv: " + str(analised_data[client_ip]['sent_and_rcv']))
print("client sent not recv: " + str(analised_data[client_ip]['sent_not_rcv']))


#print(msg_data['10.244.0.196']['recv']['A']['10.244.0.191'])
