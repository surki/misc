#include <vector>
#include <string>
#include <sstream>
#include <list>
#include <map>

#include "common/buffer/buffer_impl.h"

namespace MySQL {

class Packet;
typedef std::unique_ptr<Packet> PacketPtr;

class MySQLDecoder {
public:
  MySQLDecoder();
  ~MySQLDecoder();

  //TODO: Move functions into private
  void onClientData(Envoy::Buffer::Instance& buffer);
  void onServerData(Envoy::Buffer::Instance& buffer);
  bool handlePacket(PacketPtr& pkt);
  bool shouldProcessClientPkts();
  bool shouldProcessServerPkts();
  void handleServerHandshake(Packet& pkt);
  void handleClientHandshake(Packet& pkt);
  void handleServerResponse(Packet& pkt);
  void handleClientQuery(Packet& pkt);
  void handleQueryResponse(Packet& pkt);
  void handleLocalInfileData(Packet& pkt);
  void handleLocalInfileResult(Packet& pkt);

private:
  void resetQueryState();

  enum class PacketState { ProcessingClientPkts, ProcessingServerPkts };

  enum class ConnectionState {
    // IDLE,
    ReadServerHandshake,
    ReadClientHandshake,
    ReadServerHandshakeResponse,
    ReadClientQuery,
    ReadServerQueryResult,
    LocalInFileData,
    LocalInFileResult
  };

  enum class QueryState { Idle, ReadColumns, ReadRows };

  uint32_t capabilities_;

  ConnectionState connState_;
  QueryState queryState_;

  bool sniffing_;

  uint8_t sequenceId_;
  Envoy::Buffer::OwnedImpl clientBuffer_;
  Envoy::Buffer::OwnedImpl serverBuffer_;

  std::list<PacketPtr> clientPkts_, serverPkts_;
};

typedef std::unique_ptr<MySQLDecoder> MySQLDecoderPtr;

enum class PacketType { UnknownPacket, OkPacket, ErrPacket, EOFPacket, Progress, LocalInFileData };

class BufferHelper {
public:
  static uint64_t peekFixedInt(Envoy::Buffer::Instance& data, uint64_t size);
  static uint64_t getFixedInt(Envoy::Buffer::Instance& data, uint64_t size);
  static uint8_t peekByte(Envoy::Buffer::Instance& data);
  static uint8_t getByte(Envoy::Buffer::Instance& data);
  static void peekBytes(Envoy::Buffer::Instance& data, uint8_t* out, size_t out_len);
  static void getBytes(Envoy::Buffer::Instance& data, uint8_t* out, size_t out_len);
  static void skipBytes(Envoy::Buffer::Instance& data, size_t len);
  static uint64_t getLenEncInt(Envoy::Buffer::Instance& data);
  static uint8_t getInt8(Envoy::Buffer::Instance& data);
  static uint8_t peekInt8(Envoy::Buffer::Instance& data);
  static uint16_t getInt16(Envoy::Buffer::Instance& data);
  static uint16_t peekInt16(Envoy::Buffer::Instance& data);
  static uint32_t getInt24(Envoy::Buffer::Instance& data);
  static uint32_t peekInt24(Envoy::Buffer::Instance& data);
  static uint32_t getInt32(Envoy::Buffer::Instance& data);
  static uint32_t peekInt32(Envoy::Buffer::Instance& data);
  static std::string getCString(Envoy::Buffer::Instance& data);
  static std::string getLenPrefixedString(Envoy::Buffer::Instance& data);
  static std::string getLenEncString(Envoy::Buffer::Instance& data);
  static std::string getStringFromBuffer(Envoy::Buffer::Instance& data, size_t len);
  static std::string getStringFromRestOfBuffer(Envoy::Buffer::Instance& data);
};

class Packet {
public:
  uint8_t seqId_;
  Envoy::Buffer::OwnedImpl buffer_;
  uint32_t capabilities_;

  Packet(uint32_t capabilities);
  void fromBuffer(Envoy::Buffer::Instance& buffer);
  uint64_t length();
  uint8_t header();
  PacketType type();
  static bool containsFullPkt(Envoy::Buffer::Instance& buffer);
};

class Message {};

class ServerHandshakeMessage : public Message {
public:
  ServerHandshakeMessage();

  uint8_t protoVersion_;
  std::string srvName_;
  uint32_t threadId_;
  std::string authPluginData1_, authPluginData2_;
  uint32_t capabilities_;
  uint8_t charset_;
  uint16_t srvStatus_;
  std::string authPluginName_;

  void fromPacket(Packet& pkt);
  std::string toString();
};

class ClientHandshakeMessage : public Message {
public:
  ClientHandshakeMessage();

  uint32_t capabilities_;
  uint8_t charset_;
  uint32_t maxPktSize_;
  std::string userName_, authResp_, dbName_, authPluginName_, password_;
  std::map<std::string, std::string> connAttribs_;

  void fromPacket(Packet& pkt);
  std::string toString();
};

class OkMessage : public Message {
public:
  OkMessage();

  uint64_t affectedRows_;
  uint64_t lastInsertId_;
  uint16_t status_;
  uint16_t warnings_;
  std::string info_;
  std::string sessionStateChanges_;

  void fromPacket(Packet& pkt);
  std::string toString();
};

class ErrMessage : public Message {
public:
  ErrMessage();

  uint16_t errorCode_;
  std::string sqlMarker_;
  std::string sqlState_;
  std::string errorMsg_;

  void fromPacket(Packet& pkt);
  std::string toString();
};

class EofMessage : public Message {
public:
  EofMessage();

  uint16_t warnings_;
  uint16_t status_;

  void fromPacket(Packet& pkt);
  std::string toString();
};

class QueryMessage : public Message {
public:
  QueryMessage();

  uint8_t command_;
  std::string commandName_;
  std::string info_;

  void fromPacket(Packet& pkt);

  std::string toString();
};

}; // namespace MySQL
