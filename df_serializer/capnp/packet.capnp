@0xfe519a9b82cbbaf7;

using Cxx = import "/capnp/c++.capnp";
$Cxx.namespace("packet");

struct Peer {
    host @0 :Text;
    port @1 :Int32;
    age  @2 :Int32;
    id   @3 :Int64;
    pos  @4 :Float64;
}

struct Packet {

    type @0 :Type;
    senderIP @1 :Text;
    senderPort @2 :Int32; #será identificado para já pela porta

    payload :union {
       nothing @3 :Void;
       view @4 :List(Peer);
    }

    enum Type {
        announce @0;
        termination @1;
        normal @2;
        response @3;
        getview @4;
    }
}

#struct Person {
#  id @0 :UInt32;
#  name @1 :Text;
#  email @2 :Text;
#  phones @3 :List(PhoneNumber);

#  struct PhoneNumber {
#    number @0 :Text;
#    type @1 :Type;

#    enum Type {
#      mobile @0;
#      home @1;
#      work @2;
#    }
#  }

#  employment :union {
#    unemployed @4 :Void;
#    employer @5 :Text;
#    school @6 :Text;
#    selfEmployed @7 :Void;
#    # We assume that a person is only one of these.
#  }
#}

#struct Packet {
#  people @0 :List(Person);
#}