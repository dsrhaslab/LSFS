//
// Created by danielsf97 on 1/28/20.
//

#include <iostream>
#include <thread>
#include <chrono>
#include <signal.h>
#include <time.h>
#include "yaml-cpp/yaml.h"
#include <fstream>
#include <lsfs/fuse_lsfs/metadata.h>
#include "df_loadbalancing/load_balancer_listener.h"
#include "client.h"
#include "df_loadbalancing/dynamic_load_balancer.h"
#include "df_core/peer.h"

extern std::string merge_metadata(std::string&, std::string&);

int main(int argc, char **argv) {
    //if(argc < 3){
    //    exit(1);
    //}

    int lb_port = 50009;
    int kv_port = 50010;
    std::string ip = "127.0.0.1";
    long id = 4;

    int lb_port2 = 50007;
    int kv_port2 = 50008;
    std::string ip2 = "127.0.0.1";
    long id2 = 3;

    client cli = client(ip, "127.0.0.1", id/*, kv_port, lb_port*/, "../conf.yaml");
    // cli2 = client(ip2,"127.0.0.1", id2/*, kv_port2, lb_port2*/, "../scripts/conf.yaml");


    cli.put("/", 0, "ole", 3);
    cli.put("/ola", 0, "ole", 3);
//    cli.put("/tudo/bem", 0, "ole", 3);
//    cli.put("/crl", 0, "ole", 3);
//    cli.put("/CVS", 1, "oli", 3);
//    cli.put("/CVS", 2, "oli", 3);

//    cli.put("/crl", 1, "oli", 3);
//    cli.put("/crl", 2, "olo", 3);

//    long version;
//    try {
//        version = cli.get_latest_version("/", 1);
//    }catch(...){
//        version = 0;
//    }
//    std::cout << "OMG" << version << std::endl;

//    cli.put("/bigfileset/00000001/00000001",version + 1,
//    "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Vestibulum eget elit eget purus tristique aliquam sit amet ut dolor. Sed dapibus, orci sed laoreet sodales, urna ante dictum lacus, convallis rhoncus dolor metus at massa. Proin egestas mauris diam, eu vestibulum lectus cursus eget. Integer iaculis neque ac hendrerit consectetur. Nullam dui est, semper at commodo sit amet, mollis eu purus. Cras eget malesuada diam. Vivamus imperdiet porta libero, eu egestas lectus consectetur quis. Fusce sed pulvinar nisi, id tempus sapien. Aliquam vel risus nibh. Fusce vulputate congue ligula, non elementum enim consequat sit amet. Aliquam finibus at elit ac varius. Ut magna ipsum, sodales id aliquet quis, varius nec ligula. Quisque auctor fringilla mi eget hendrerit. Quisque est orci, eleifend nec turpis eget, condimentum blandit urna.\n"
//    "\n"
//    "Aliquam bibendum semper tortor a mattis. Sed ullamcorper, orci nec placerat laoreet, tellus arcu sagittis ex, sit amet cursus enim diam quis est. Curabitur consequat velit et ex rutrum, semper sollicitudin risus mattis. Integer sollicitudin, arcu eget porta porta, urna lorem feugiat lacus, quis dictum elit ante in nunc. Vestibulum nec mi a elit pellentesque porttitor nec sit amet nisi. Ut gravida ipsum leo, quis vulputate ligula congue vitae. Ut vitae urna in nibh ornare bibendum quis suscipit tellus. Vestibulum mollis lorem nulla, eget ornare tortor aliquam et. Vivamus quis mi sit amet mauris imperdiet convallis. Phasellus lorem ipsum, lacinia eu mollis vitae, lacinia euismod velit. Integer sit amet rutrum tellus, et feugiat tortor. Suspendisse dictum dapibus purus a aliquam. Nulla pretium ultrices dolor, semper lobortis ipsum tempor vitae. Praesent imperdiet lectus quis neque finibus varius. Sed lectus velit, rhoncus sed fermentum a, tincidunt pellentesque lacus. Morbi tempus ante vel massa tristique, lobortis pharetra massa rutrum.\n"
//    "\n"
//    "Sed et nunc ut eros feugiat fermentum quis vitae felis. Mauris felis quam, eleifend non urna sit amet, feugiat sodales arcu. Nulla commodo pharetra est nec tempor. Aenean sollicitudin sapien eget tempus tincidunt. Nunc non arcu non arcu mattis elementum. Sed euismod orci non hendrerit gravida. Fusce molestie vestibulum tortor vitae luctus. Cras nulla nisl, efficitur nec egestas ut, interdum sit amet turpis. Maecenas congue viverra quam, in pharetra mi pharetra sed.\n"
//    "\n"
//    "Nunc egestas dui at faucibus gravida. In hac habitasse platea dictumst. Pellentesque facilisis varius nibh in auctor. In id sem vestibulum, condimentum ligula ut, tincidunt risus. Suspendisse mollis dolor in auctor porttitor. Maecenas lorem risus, elementum at porta volutpat, volutpat in orci. Nam ultricies, risus a eleifend egestas, lacus dui mollis turpis, a suscipit est risus ac massa. In tristique est velit, in imperdiet tellus tempus a. Nunc eget lorem molestie, efficitur enim nec, dignissim erat. Nam et ipsum ante. Duis ut massa risus. Integer aliquet eros elit, sed semper nisl tempor id. Praesent accumsan faucibus condimentum. Vivamus auctor tempus nulla sit amet porta. Cras vel volutpat ante, ut vulputate metus.\n"
//    "\n"
//    "Aenean sit amet pulvinar justo. Sed et ipsum varius, facilisis arcu eget, auctor sem. Ut varius eros varius purus rutrum, at dictum lorem rhoncus. Cras et nunc quis metus sagittis convallis. Curabitur semper enim at lacus fermentum, quis bibendum tellus ornare. In enim libero, pharetra in lacinia eget, vulputate at lorem. Aenean iaculis neque quis facilisis aliquet. Morbi elementum, ligula a varius blandit, ante dolor vulputate nisi, vestibulum finibus tortor mauris ut risus. Mauris finibus enim sed consequat accumsan. Aliquam volutpat, sem hendrerit auctor rhoncus, leo nunc lobortis ligula, quis sollicitudin massa odio euismod purus. Proin vestibulum sed nibh placerat faucibus. Vestibulum tortor felis, rhoncus aliquet hendrerit id, commodo ut ipsum. "
//    ,100);

//    try {
//        version = cli.get_latest_version("/bigfileset/00000001/00000001", 1);
//    }catch(...){
//        version = 0;
//    }
//
//    cli.put("/bigfileset/00000001/00000001",version + 1,"KV ALMOST FINISHED", 18, 1);
//    long version = 0;
//    cli.put("/bigfileset/00000001/00000001",version,"KV ALMOST FINISHED", 18, 1);
////    cli.put(3,1,"KV ALMOST FINISHED");
////    cli.put(4,1,"KV ALMOST FINISHED");

//    try {
//        version = cli.get_latest_version("/bigfileset/00000001/00000001", 1);
//    }catch(...){
//        version = 0;
//    }

//    std::shared_ptr<std::string> data = cli.get("/bigfileset/00000001/00000001",1, &version);
//    std::string data_s = *data;

//    std::shared_ptr<std::string> data1 = cli.get("/bigfileset/00000001/00000001",1);
//    std::string data1_s = *data1;
//    std::cout << data1_s << std::endl;
//
//    std::cout << "PUT DONE !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!" << std::endl;

/*
    struct stat stbuf;
    metadata::initialize_metadata(&stbuf, 301, 2, 0, 1000);
    metadata met(stbuf);
    met.add_child("/a.txt", false);
    met.add_child("/b.txt", false);

    met.reset_add_remove_log();

    metadata met1 = met;
    metadata met2 = met;

    met1.add_child("/a", true);

    met2.add_child("/b", true);

    std::string bytes_met1 = metadata::serialize_to_string(met1);
    cli.put_with_merge("/",1, bytes_met1.data(), bytes_met1.size(), 1);

//    std::string bytes_met2 = metadata::serialize_to_string(met2);
//    cli2.put_with_merge("/",1, bytes_met2.data(), bytes_met2.size(), 1);

//    std::string res = merge_metadata(bytes_met1, bytes_met2);

    std::shared_ptr<std::string> data_res = cli.get("/",1);


    metadata met3 = metadata::deserialize_from_string(*data_res);
    std::cout << met3.stbuf.st_nlink << std::endl;
*/
    cli.stop();

    return 0;
}