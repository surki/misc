#include "tins/sniffer.h"
#include "tins/tcp_ip/stream_follower.h"

#include <iostream>
#include <stdexcept>
#include <string>

#include "codec.h"

using Tins::Packet;
using Tins::Sniffer;
using Tins::TCPIP::Stream;
using Tins::TCPIP::StreamFollower;

using namespace Tins;
using namespace Envoy;

int main() {

  // TODO: Tests
  //  - Initial handshake
  //    - Auth failure
  //    - Auth success
  //      - Auth fast path
  //      - Auth switch auth method
  //      - Other auths
  //  - Query: All commands
  //  - Query: large payload
  //    - Payload exactly 16MB
  //    - Payload > 16MB but < 32MB
  //    - Payload exactly 32MB
  //    - Payload 100MB
  //  - Query: Prepared statements
  //  
  
  try {
    FileSniffer sniffer("/tmp/test.pcap");

    StreamFollower follower;
    follower.new_stream_callback([](Stream& stream) {
        std::shared_ptr<MySQL::MySQLDecoder> decoder = std::make_shared<MySQL::MySQLDecoder>();

        std::shared_ptr<Envoy::Buffer::OwnedImpl> rb = std::make_shared<Envoy::Buffer::OwnedImpl>();
        stream.client_data_callback([decoder, rb](Stream& stream) {
            auto p = stream.client_payload();
            rb->add(&p[0], p.size());
            decoder->onClientData(*rb);
            rb->drain(rb->length());

            // // Feed one byte at a time.
            // for (int i=0; i < p.size(); i++) {
            //   rb->add(&p[i], 1);
            //   decoder->onClientData(*rb);
            //   rb->drain(rb->length());
            // }
          });

        std::shared_ptr<Envoy::Buffer::OwnedImpl> wb = std::make_shared<Envoy::Buffer::OwnedImpl>();
        stream.server_data_callback([decoder, wb](Stream& stream){
            auto p = stream.server_payload();
            wb->add(&p[0], p.size());
            decoder->onServerData(*wb);
            wb->drain(wb->length());

            // // Feed one byte at a time.
            // for (int i=0; i < p.size(); i++) {
            //   wb->add(&p[i], 1);
            //   decoder->onServerData(*wb);
            //   wb->drain(wb->length());
            // }
          });


        stream.auto_cleanup_payloads(true);
      });

    sniffer.sniff_loop([&](Packet& packet) {
        follower.process_packet(packet);
        return true;
      });
  } catch (std::exception& ex) {
    std::cerr << "Error: " << ex.what() << std::endl;
    return 1;
  }
}
