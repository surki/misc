#include <iostream>
#include <cstring>
#include <assert.h>

#include "codec.h"
#include "fmt/printf.h"
#include "exception.h"

#include "mysql.h"

#define ENVOY_LOG(LEVEL, ...)                      \
  do {                                             \
    fmt::print(__VA_ARGS__);                       \
  } while (0)

using namespace Envoy;
using namespace std;

namespace MySQL {
 
MySQLDecoder::MySQLDecoder()
  : sniffing_(true), sequenceId_(0), connState_(ConnectionState::ReadServerHandshake),
    queryState_(QueryState::Idle) {}

MySQLDecoder::~MySQLDecoder() { }

void MySQLDecoder::onClientData(Buffer::Instance& buffer) {
  if (!sniffing_) {
    return;
  }

  clientBuffer_.add(buffer);

  while (Packet::containsFullPkt(clientBuffer_)) {
    if (!clientPkts_.empty() && clientPkts_.back()->moreData_) {
      clientPkts_.back()->fromBuffer(clientBuffer_);
    } else {
      PacketPtr pkt = std::make_unique<Packet>(capabilities_);
      pkt->fromBuffer(clientBuffer_);
      clientPkts_.push_back(std::move(pkt));
    }

    if (sequenceId_ != clientPkts_.back()->seqId_) {
      throw EnvoyException(fmt::format("Wrong sequence ID from client. Expected {} but got {}",
                                       sequenceId_, clientPkts_.back()->seqId_));
    }

    sequenceId_++;
  }

  while (!clientPkts_.empty() && !clientPkts_.front()->moreData_ && shouldProcessClientPkts()) {
    handlePacket(clientPkts_.front());
    clientPkts_.pop_front();
  }
}

void MySQLDecoder::onServerData(Buffer::Instance& buffer) {
  if (!sniffing_) {
    return;
  }
    
  serverBuffer_.add(buffer);

  while (Packet::containsFullPkt(serverBuffer_)) {
    if (!serverPkts_.empty() && serverPkts_.back()->moreData_) {
      serverPkts_.back()->fromBuffer(serverBuffer_);
    } else {
      PacketPtr pkt = std::make_unique<Packet>(capabilities_);
      pkt->fromBuffer(serverBuffer_);
      serverPkts_.push_back(std::move(pkt));
    }

    if (sequenceId_ != serverPkts_.back()->seqId_) {
      throw EnvoyException(fmt::format("Wrong sequence ID from server. Expected {} but got {}",
                                       sequenceId_, clientPkts_.back()->seqId_));
    }
    sequenceId_++;
  }

  while (!serverPkts_.empty() && !serverPkts_.front()->moreData_ && shouldProcessServerPkts()) {
    handlePacket(serverPkts_.front());
    serverPkts_.pop_front();
  }
}

bool MySQLDecoder::handlePacket(PacketPtr& pkt) {

  switch (connState_) {
  case ConnectionState::ReadServerHandshake:
    ENVOY_LOG(trace, "Read server handshake\n");
    handleServerHandshake(*pkt);
    break;
  case ConnectionState::ReadClientHandshake:
    ENVOY_LOG(trace, "Read client handshake\n");
    handleClientHandshake(*pkt);
    break;
  case ConnectionState::ReadServerHandshakeResponse:
    handleServerResponse(*pkt);
    break;
  case ConnectionState::ReadClientQuery:
    handleClientQuery(*pkt);
    break;
  case ConnectionState::ReadServerQueryResult:
    handleQueryResponse(*pkt);
    break;
  case ConnectionState::LocalInFileData:
    handleLocalInfileData(*pkt);
    break;
  case ConnectionState::LocalInFileResult:
    handleLocalInfileResult(*pkt);
    break;
  default:
    throw EnvoyException(fmt::format("Unknown connection state {}", connState_));
  }

  return true;
}

bool MySQLDecoder::shouldProcessClientPkts() {
  return connState_ == ConnectionState::ReadClientHandshake ||
    connState_ == ConnectionState::ReadClientQuery ||
    connState_ == ConnectionState::LocalInFileData;
}

bool MySQLDecoder::shouldProcessServerPkts() {
  return connState_ == ConnectionState::ReadServerHandshake ||
    connState_ == ConnectionState::ReadServerHandshakeResponse ||
    connState_ == ConnectionState::ReadServerQueryResult ||
    connState_ == ConnectionState::LocalInFileData ||
    connState_ == ConnectionState::LocalInFileResult;
}

void MySQLDecoder::handleServerHandshake(Packet& pkt) {
  ENVOY_LOG(trace, "Server handshake message: Seqid: {} Len: {}\n", pkt.seqId_, pkt.length());

  // TODO: Handle err packet from server

  ServerHandshakeMessage msg;
  msg.fromPacket(pkt);
  capabilities_ = msg.capabilities_;

  ENVOY_LOG(trace, "{}", msg.toString());

  connState_ = ConnectionState::ReadClientHandshake;
}

void MySQLDecoder::handleClientHandshake(Packet& pkt) {
  ENVOY_LOG(trace, "Client handshake message: Seqid: {} Len: {}\n", pkt.seqId_, pkt.length());

  // TODO handle SSL_Request_Packet and other types 

  ClientHandshakeMessage msg;
  msg.fromPacket(pkt);

  capabilities_ = msg.capabilities_;

  ENVOY_LOG(trace, "{}", msg.toString());

  connState_ = ConnectionState::ReadServerHandshakeResponse;
}

void MySQLDecoder::handleServerResponse(Packet& pkt) {
  ENVOY_LOG(trace, "Server handshake response message: Seqid: {} Len: {}\n", pkt.seqId_,
            pkt.length());

  auto pkt_type = pkt.type();
  switch (pkt_type) {
  case PacketType::OkPacket: {
    ENVOY_LOG(trace, "OK Packet\n");
    OkMessage msg;
    msg.fromPacket(pkt);
    ENVOY_LOG(trace, "{}", msg.toString());
    break;
  }
  case PacketType::ErrPacket: {
    ENVOY_LOG(trace, "Err Packet\n");
    ErrMessage msg;
    msg.fromPacket(pkt);
    ENVOY_LOG(trace, "{}", msg.toString());
    break;
  }
  case PacketType::EOFPacket: {
    ENVOY_LOG(trace, "EOF Packet\n");
    EofMessage msg;
    msg.fromPacket(pkt);
    ENVOY_LOG(trace, "{}", msg.toString());
    break;
  }
  default:
    ENVOY_LOG(trace, "Unknown Packet\n");
    assert(false);
    break;
  }

  resetQueryState();
}

void MySQLDecoder::handleClientQuery(Packet& pkt) {
  Buffer::Instance& buffer = pkt.buffer_;
  ENVOY_LOG(trace, "Query from client: Seqid: {} Len: {}\n", pkt.seqId_, pkt.length());

  QueryMessage msg;
  msg.fromPacket(pkt);
  ENVOY_LOG(trace, "{}", msg.toString());

  connState_ = ConnectionState::ReadServerQueryResult;
  queryState_ = QueryState::ReadColumns;
}

void MySQLDecoder::handleQueryResponse(Packet& pkt) {
  Buffer::Instance& buffer = pkt.buffer_;

  ENVOY_LOG(trace, "Query response from server: Seqid: {} Len: {}\n", pkt.seqId_, pkt.length());

  switch (queryState_) {
  case QueryState::ReadColumns: {
    auto pkt_type = pkt.type();
    switch (pkt_type) {
    case PacketType::OkPacket: {
      ENVOY_LOG(trace, "OK Packet\n");
      OkMessage msg;
      msg.fromPacket(pkt);
      ENVOY_LOG(trace, "{}", msg.toString());

      resetQueryState();
      break;
    }
    case PacketType::ErrPacket: {
      ENVOY_LOG(trace, "Err Packet\n");
      ErrMessage msg;
      msg.fromPacket(pkt);
      ENVOY_LOG(trace, "{}", msg.toString());

      resetQueryState();
      break;
    }
    case PacketType::EOFPacket: {
      ENVOY_LOG(trace, "EOF Packet\n");
      EofMessage msg;
      msg.fromPacket(pkt);
      ENVOY_LOG(trace, "{}", msg.toString());

      queryState_ = QueryState::ReadRows;
      break;
    }
    case PacketType::LocalInFileData: {
      ENVOY_LOG(trace, "LocalInFile request");
      connState_ = ConnectionState::LocalInFileData;
      break;
    }
    default: {
      uint8_t h = pkt.header();
      ENVOY_LOG(trace, "Packet header {}\n", h);
      // if (h == LOCAL_INFILE) {
      //   connState_ = ConnectionState::LocalInFile;
      //   break;
      //   //throw EnvoyException(fmt::format("LOCAL_INFILE not supported yet"));
      // }

      uint64_t n = BufferHelper::getLenEncInt(buffer);
      ENVOY_LOG(trace, "Result set: Length: {}\n", n);
      break;
    }
    }

    break;
  }
  case QueryState::ReadRows: {
    auto pkt_type = pkt.type();
    switch (pkt_type) {
    case PacketType::OkPacket: {
      ENVOY_LOG(trace, "OK Packet\n");
      OkMessage msg;
      msg.fromPacket(pkt);
      ENVOY_LOG(trace, "{}", msg.toString());

      resetQueryState();
      break;
    }
    case PacketType::ErrPacket: {
      ENVOY_LOG(trace, "Err Packet\n");
      ErrMessage msg;
      msg.fromPacket(pkt);
      ENVOY_LOG(trace, "{}", msg.toString());

      resetQueryState();
      break;
    }
    case PacketType::EOFPacket: {
      ENVOY_LOG(trace, "EOF Packet\n");
      EofMessage msg;
      msg.fromPacket(pkt);
      ENVOY_LOG(trace, "{}", msg.toString());

      resetQueryState();
      break;
    }
    default: {
      uint8_t h = pkt.header();
      if (h == LOCAL_INFILE && pkt.length() > 1) {
        throw EnvoyException(fmt::format("LOCAL_INFILE response not supported yet"));
        break;
      }

      ENVOY_LOG(trace, "Rows\n");
      break;
    }
    }

    break;
  }
  default:
    assert(false);
    break;
  }
}

void MySQLDecoder::handleLocalInfileData(Packet& pkt) {
  ENVOY_LOG(trace, "LocalInFile Data : Seqid: {} Len: {}\n", pkt.seqId_, pkt.length());
  if (pkt.length() == 0 ) {
    connState_ = ConnectionState::LocalInFileResult;
    return;
  }
}

void MySQLDecoder::handleLocalInfileResult(Packet& pkt) {
  auto pkt_type = pkt.type();
  switch (pkt_type) {
  case PacketType::OkPacket: {
    ENVOY_LOG(trace, "OK Packet\n");
    OkMessage msg;
    msg.fromPacket(pkt);
    ENVOY_LOG(trace, "{}", msg.toString());

    resetQueryState();
    break;
  }
  case PacketType::ErrPacket: {
    ENVOY_LOG(trace, "Err Packet\n");
    ErrMessage msg;
    msg.fromPacket(pkt);
    ENVOY_LOG(trace, "{}", msg.toString());

    resetQueryState();
    break;
  }
  case PacketType::EOFPacket: {
    ENVOY_LOG(trace, "EOF Packet\n");
    EofMessage msg;
    msg.fromPacket(pkt);
    ENVOY_LOG(trace, "{}", msg.toString());

    resetQueryState();
    break;
  }
  case PacketType::Progress: {
    ENVOY_LOG(trace, "Progress packet\n");

    ErrMessage msg;
    msg.fromPacket(pkt);
    ENVOY_LOG(trace, "{}", msg.toString());
    break;
  }
  default: {
    throw EnvoyException(fmt::format("Unknown LocalInFile response: {}", pkt_type));
    break;
  }
  }
}

void MySQLDecoder::resetQueryState() {
  connState_ = ConnectionState::ReadClientQuery;
  queryState_ = QueryState::Idle;
  sequenceId_ = 0;
}

// https://dev.mysql.com/doc/internals/en/basic-types.html

uint64_t BufferHelper::peekFixedInt(Buffer::Instance& data, uint64_t size) {
  if (data.length() < size) {
    throw EnvoyException(fmt::format("Invalid buffer size: {} {}", data.length(), size));
  }

  uint8_t* mem = reinterpret_cast<uint8_t*>(data.linearize(size));
  uint64_t val = 0;
  for (int i = 0, s = 0; i < size; i++, s += 8) {
    val |= mem[i] << s;
  }

  return val;
}

uint64_t BufferHelper::getFixedInt(Buffer::Instance& data, uint64_t size) {
  auto val = peekFixedInt(data, size);
  data.drain(size);
  return val;
}

uint8_t BufferHelper::peekByte(Buffer::Instance& data) {
  if (data.length() == 0) {
    throw EnvoyException(fmt::format("Invalid buffer size, buffer is empty"));
  }

  void* mem = data.linearize(sizeof(uint8_t));
  return *reinterpret_cast<uint8_t*>(mem);
}

uint8_t BufferHelper::getByte(Buffer::Instance& data) {
  if (data.length() == 0) {
    throw EnvoyException(fmt::format("Invalid buffer size, buffer is empty"));
  }

  void* mem = data.linearize(sizeof(uint8_t));
  uint8_t ret = *reinterpret_cast<uint8_t*>(mem);
  data.drain(sizeof(uint8_t));
  return ret;
}

void BufferHelper::peekBytes(Buffer::Instance& data, uint8_t* out, size_t out_len) {
  if (data.length() < out_len) {
    throw EnvoyException(fmt::format("Invalid buffer size: {} {}", data.length(), out_len));
  }

  void* mem = data.linearize(out_len);
  std::memcpy(out, mem, out_len);
}

void BufferHelper::getBytes(Buffer::Instance& data, uint8_t* out, size_t out_len) {
  if (data.length() < out_len) {
    throw EnvoyException(fmt::format("Invalid buffer size: {} {}", data.length(), out_len));
  }

  void* mem = data.linearize(out_len);
  std::memcpy(out, mem, out_len);
  data.drain(out_len);
}

void BufferHelper::skipBytes(Buffer::Instance& data, size_t len) {
  if (data.length() < len) {
    throw EnvoyException(fmt::format("Invalid buffer size: {} {}", data.length(), len));
  }

  data.drain(len);
}

uint64_t BufferHelper::getLenEncInt(Buffer::Instance& data) {
  uint8_t len = getByte(data);
  if (len <= 0) {
    return 0;
  }

  uint8_t size = 0;
  if (len < 0xfb) {
    return len;
  } else if (len == 0xfc) {
    size = 2;
  } else if (len == 0xfd) {
    size = 3;
  } else if (len == 0xfe) {
    size = 8;
  } else {
    throw EnvoyException(fmt::format("Unknown length encoded int: {}", len));
  }

  std::vector<uint8_t> mem(size);

  getBytes(data, &mem[0], size);

  uint64_t val = 0;
  for (int i = 0, s = 0; i < size; i++, s += 8) {
    val |= mem[i] << s;
  }

  return val;
}

uint8_t BufferHelper::getInt8(Buffer::Instance& data) { return getFixedInt(data, 1); }

uint8_t BufferHelper::peekInt8(Buffer::Instance& data) { return peekFixedInt(data, 1); }

uint16_t BufferHelper::getInt16(Buffer::Instance& data) { return getFixedInt(data, 2); }

uint16_t BufferHelper::peekInt16(Buffer::Instance& data) { return peekFixedInt(data, 2); }

uint32_t BufferHelper::getInt24(Buffer::Instance& data) { return getFixedInt(data, 3); }

uint32_t BufferHelper::peekInt24(Buffer::Instance& data) { return peekFixedInt(data, 3); }

uint32_t BufferHelper::getInt32(Buffer::Instance& data) { return getFixedInt(data, 4); }

uint32_t BufferHelper::peekInt32(Buffer::Instance& data) { return peekFixedInt(data, 4); }

std::string BufferHelper::getCString(Buffer::Instance& data) {
  char end = '\0';
  ssize_t index = data.search(&end, sizeof(end), 0);
  if (index == -1) {
    throw EnvoyException(fmt::format("Invalid CString"));
  }

  char* start = reinterpret_cast<char*>(data.linearize(index + 1));
  std::string ret(start);
  data.drain(index + 1);
  return ret;
}

std::string BufferHelper::getLenPrefixedString(Buffer::Instance& data) {
  uint64_t size = getInt8(data);
  if (size == 0) {
    throw EnvoyException(fmt::format("Invalid length prefixed string"));
  }

  std::string s;
  s.reserve(size);
  assert(s.capacity() >= size);
  getBytes(data, reinterpret_cast<uint8_t *>(&s[0]), size);
  s.resize(size);

  return s;
}

std::string BufferHelper::getLenEncString(Buffer::Instance& data) {
  uint64_t size = getLenEncInt(data);
  if (size == 0) {
    return std::string();
  }

  std::string s;
  s.reserve(size);
  assert(s.capacity() >= size);
  getBytes(data, reinterpret_cast<uint8_t *>(&s[0]), size);
  s.resize(size);

  return s;

  // // TODO: Replace it with string.reserve()
  // std::vector<uint8_t> mem(size);

  // getBytes(data, &mem[0], size);

  // return std::string(mem.begin(), mem.end());
}

std::string BufferHelper::getStringFromBuffer(Buffer::Instance& data, size_t len) {
  if (len <= 0) {
    return std::string();
  }

  std::vector<uint8_t> b(len);
  BufferHelper::getBytes(data, &b[0], len);
  return std::string(b.begin(), b.end());
}

std::string BufferHelper::getStringFromRestOfBuffer(Buffer::Instance& data) {
  return getStringFromBuffer(data, data.length());
}

Packet::Packet(uint32_t capabilities) : seqId_(0), moreData_(false), capabilities_(capabilities) {}

void Packet::fromBuffer(Buffer::Instance& buffer) {
  auto length = BufferHelper::getInt24(buffer);

  seqId_ = BufferHelper::getInt8(buffer);
  // ENVOY_LOG(trace, "=== {} = {}\n", length, buffer.length());
  assert(buffer.length() >= length);

  buffer_.move(buffer, length);

  moreData_ = ((length+4) >= MAX_PAYLOAD_LEN);

  return;
}

uint64_t Packet::length() { return buffer_.length(); }

uint8_t Packet::header() { return BufferHelper::peekInt8(buffer_); }

PacketType Packet::type() {
  uint8_t header = BufferHelper::peekInt8(buffer_);
  if (header == OK_HEADER && buffer_.length() >= 7) {
    return PacketType::OkPacket;
  } else if (header == EOF_HEADER && buffer_.length() <= 9) {
    return PacketType::EOFPacket;
  } else if (header == ERR_HEADER) {
    auto error_code = BufferHelper::peekInt16(buffer_);
    if (error_code == 0xFFFF) {
      return PacketType::Progress;
    }
    return PacketType::ErrPacket;
  } else if (header == LOCAL_INFILE) {
    return PacketType::LocalInFileData;
  }

  return PacketType::UnknownPacket;
}

bool Packet::containsFullPkt(Buffer::Instance& buffer) {
  // First 3 bytes contain packet length, followed by 1 byte sequence id.
  if (buffer.length() < sizeof(uint32_t)) {
    return false;
  }

  // Get the packet length
  uint32_t pkt_len = BufferHelper::peekFixedInt(buffer, 3);
  if (buffer.length() < (pkt_len + sizeof(uint32_t))) {
    return false;
  }

  return true;
}

ServerHandshakeMessage::ServerHandshakeMessage() {}

void ServerHandshakeMessage::fromPacket(Packet& pkt) {
  Buffer::OwnedImpl& buffer = pkt.buffer_;

  protoVersion_ = BufferHelper::getInt8(buffer);
  srvName_ = BufferHelper::getCString(buffer);
  threadId_ = BufferHelper::getInt32(buffer);
  authPluginData1_ = BufferHelper::getCString(buffer);
  capabilities_ = BufferHelper::getInt16(buffer);
  charset_ = BufferHelper::getInt8(buffer);
  srvStatus_ = BufferHelper::getInt16(buffer);
  capabilities_ |= (BufferHelper::getInt16(buffer) << 16);

  uint32_t auth_plugin_data_len = BufferHelper::getInt8(buffer);

  BufferHelper::skipBytes(buffer, 10);

  if ((capabilities_ & CLIENT_PLUGIN_AUTH) || (capabilities_ & CLIENT_SECURE_CONNECTION)) {
    // We are guaranteed to have at least 13 bytes if
    // CLIENT_SECURE_CONNECTION is set. If CLIENT_PLUGIN_AUTH is set, then
    // we will have data of auth_plugin_data_len length.  Probably we can
    // just use BufferHelper::getCString() here?
    // TODO:
    size_t len = std::max(static_cast<uint32_t>(13), auth_plugin_data_len - 8);
    authPluginData2_ = BufferHelper::getStringFromBuffer(buffer, len);
  }

  if (capabilities_ & CLIENT_PLUGIN_AUTH) {
    try {
      authPluginName_ = BufferHelper::getCString(buffer);
    } catch (...) {
      // Looks like in certain MySQL versions, due to a bug,
      // auth_plugin_name may miss trailing '\0'.  So we will read
      // rest of the data.
      authPluginName_ = BufferHelper::getStringFromRestOfBuffer(buffer);
    }
  }

  // TODO: Perform some validations on auth_plugin_data length etc
  // based on CLIENT_PLUGIN_AUTH or CLIENT_SECURE_CONNECTION
}

std::string ServerHandshakeMessage::toString() {
  std::stringstream s;

  s << "Protocol version: " << protoVersion_ << std::endl;
  s << "Server name is: " << srvName_ << std::endl;
  s << "Thread id: " << threadId_ << std::endl;
  s << "auth_plugin_data1: " << authPluginData1_ << std::endl;
  s << "Server language: " << collations[charset_] << std::endl;
  s << "Server status: " << srvStatus_ << std::endl;
  s << "\t Server status In Trans           : "
    << ((srvStatus_ & SERVER_STATUS_IN_TRANS) ? string("SET") : string("NOT SET")) << std::endl;
  s << "\t Server status Autocommit         : "
    << ((srvStatus_ & SERVER_STATUS_AUTOCOMMIT) ? string("SET") : string("NOT SET")) << std::endl;
  s << "\t Server status More results       : "
    << ((srvStatus_ & SERVER_MORE_RESULTS_EXISTS) ? string("SET") : string("NOT SET")) << std::endl;
  s << "\t Server status No good index used : "
    << ((srvStatus_ & SERVER_QUERY_NO_GOOD_INDEX_USED) ? string("SET") : string("NOT SET"))
    << std::endl;
  s << "\t Server status No index used      : "
    << ((srvStatus_ & SERVER_QUERY_NO_INDEX_USED) ? string("SET") : string("NOT SET")) << std::endl;
  s << "\t Server status Cursor exists      : "
    << ((srvStatus_ & SERVER_STATUS_CURSOR_EXISTS) ? string("SET") : string("NOT SET"))
    << std::endl;
  s << "\t Server status Last row sent      : "
    << ((srvStatus_ & SERVER_STATUS_LAST_ROW_SENT) ? string("SET") : string("NOT SET"))
    << std::endl;
  s << "\t Server status DB dropped         : "
    << ((srvStatus_ & SERVER_STATUS_DB_DROPPED) ? string("SET") : string("NOT SET")) << std::endl;
  s << "\t Server status No backlash escapes: "
    << ((srvStatus_ & SERVER_STATUS_NO_BACKSLASH_ESCAPES) ? string("SET") : string("NOT SET"))
    << std::endl;
  s << "\t Server status Metadata changed   : "
    << ((srvStatus_ & SERVER_STATUS_METADATA_CHANGED) ? string("SET") : string("NOT SET"))
    << std::endl;
  s << "\t Server status Query was slow     : "
    << ((srvStatus_ & SERVER_QUERY_WAS_SLOW) ? string("SET") : string("NOT SET")) << std::endl;
  s << "\t Server status PS out params      : "
    << ((srvStatus_ & SERVER_PS_OUT_PARAMS) ? string("SET") : string("NOT SET")) << std::endl;

  s << "Capabilities: " << capabilities_ << std::endl;
  s << "\t Client long password                 : "
    << ((capabilities_ & CLIENT_LONG_PASSWORD) ? string("SET") : string("NOT SET")) << std::endl;
  s << "\t Client found rows                    : "
    << ((capabilities_ & CLIENT_FOUND_ROWS) ? string("SET") : string("NOT SET")) << std::endl;
  s << "\t Client long flags                    : "
    << ((capabilities_ & CLIENT_LONG_FLAG) ? string("SET") : string("NOT SET")) << std::endl;
  s << "\t Client connect with DB               : "
    << ((capabilities_ & CLIENT_CONNECT_WITH_DB) ? string("SET") : string("NOT SET")) << std::endl;
  s << "\t Client no schema                     : "
    << ((capabilities_ & CLIENT_NO_SCHEMA) ? string("SET") : string("NOT SET")) << std::endl;
  s << "\t Client compress                      : "
    << ((capabilities_ & CLIENT_COMPRESS) ? string("SET") : string("NOT SET")) << std::endl;
  s << "\t Client ODBC                          : "
    << ((capabilities_ & CLIENT_ODBC) ? string("SET") : string("NOT SET")) << std::endl;
  s << "\t Client local files                   : "
    << ((capabilities_ & CLIENT_LOCAL_FILES) ? string("SET") : string("NOT SET")) << std::endl;
  s << "\t Client ignore space                  : "
    << ((capabilities_ & CLIENT_IGNORE_SPACE) ? string("SET") : string("NOT SET")) << std::endl;
  s << "\t Client protocol 41                   : "
    << ((capabilities_ & CLIENT_PROTOCOL_41) ? string("SET") : string("NOT SET")) << std::endl;
  s << "\t Client interactive                   : "
    << ((capabilities_ & CLIENT_INTERACTIVE) ? string("SET") : string("NOT SET")) << std::endl;
  s << "\t Client SSL                           : "
    << ((capabilities_ & CLIENT_SSL) ? string("SET") : string("NOT SET")) << std::endl;
  s << "\t Client ignore sigpipe                : "
    << ((capabilities_ & CLIENT_IGNORE_SIGPIPE) ? string("SET") : string("NOT SET")) << std::endl;
  s << "\t Client transactions                  : "
    << ((capabilities_ & CLIENT_TRANSACTIONS) ? string("SET") : string("NOT SET")) << std::endl;
  s << "\t Client reserved                      : "
    << ((capabilities_ & CLIENT_RESERVED) ? string("SET") : string("NOT SET")) << std::endl;
  s << "\t Client secure connection             : "
    << ((capabilities_ & CLIENT_SECURE_CONNECTION) ? string("SET") : string("NOT SET"))
    << std::endl;
  s << "\t Client multi statements              : "
    << ((capabilities_ & CLIENT_MULTI_STATEMENTS) ? string("SET") : string("NOT SET")) << std::endl;
  s << "\t Client multi results                 : "
    << ((capabilities_ & CLIENT_MULTI_RESULTS) ? string("SET") : string("NOT SET")) << std::endl;
  s << "\t Client PS multi results              : "
    << ((capabilities_ & CLIENT_PS_MULTI_RESULTS) ? string("SET") : string("NOT SET")) << std::endl;
  s << "\t Client plugin auth                   : "
    << ((capabilities_ & CLIENT_PLUGIN_AUTH) ? string("SET") : string("NOT SET")) << std::endl;
  s << "\t Client connect attrs                 : "
    << ((capabilities_ & CLIENT_CONNECT_ATTRS) ? string("SET") : string("NOT SET")) << std::endl;
  s << "\t Client plugin auth lenec data        : "
    << ((capabilities_ & CLIENT_PLUGIN_AUTH_LENENC_CLIENT_DATA) ? string("SET") : string("NOT SET"))
    << std::endl;
  s << "\t Client can handle expired passwords  : "
    << ((capabilities_ & CLIENT_CAN_HANDLE_EXPIRED_PASSWORDS) ? string("SET") : string("NOT SET"))
    << std::endl;
  s << "\t Client progress                      : "
    << ((capabilities_ & CLIENT_PROGRESS) ? string("SET") : string("NOT SET")) << std::endl;
  s << "\t Client SSL verify server cert        : "
    << ((capabilities_ & CLIENT_SSL_VERIFY_SERVER_CERT) ? string("SET") : string("NOT SET"))
    << std::endl;
  s << "\t Client remember options              : "
    << ((capabilities_ & CLIENT_REMEMBER_OPTIONS) ? string("SET") : string("NOT SET")) << std::endl;
  // s << "\t Client : " << (cap & ) ? "SET" : string("NOT SET") << std::endl;

  s << "auth_plugin_name: " << authPluginName_ << std::endl;
  s << "auth_plugin_data2: " << authPluginData2_ << std::endl;

  return s.str();
}

ClientHandshakeMessage::ClientHandshakeMessage() {}

void ClientHandshakeMessage::fromPacket(Packet& pkt) {
  Buffer::OwnedImpl& buffer = pkt.buffer_;

  capabilities_ = BufferHelper::getInt16(buffer);

  if (capabilities_ & CLIENT_PROTOCOL_41) {
    capabilities_ |= (BufferHelper::getInt16(buffer) << 16);
    maxPktSize_ = BufferHelper::getInt32(buffer);
    charset_ = BufferHelper::getInt8(buffer);

    BufferHelper::skipBytes(buffer, 23);

    userName_ = BufferHelper::getCString(buffer);

    if (capabilities_ & CLIENT_PLUGIN_AUTH_LENENC_CLIENT_DATA) {
      authResp_ = BufferHelper::getLenEncString(buffer);
    } else if (capabilities_ & CLIENT_SECURE_CONNECTION) {
      authResp_ = BufferHelper::getLenPrefixedString(buffer);
    } else {
      authResp_ = BufferHelper::getCString(buffer);
    }

    if (capabilities_ & CLIENT_CONNECT_WITH_DB) {
      dbName_ = BufferHelper::getCString(buffer);
    }

    if (capabilities_ & CLIENT_PLUGIN_AUTH) {
      authPluginName_ = BufferHelper::getCString(buffer);
    }

    if (capabilities_ & CLIENT_CONNECT_ATTRS) {
      string key, val;
      uint64_t len = BufferHelper::getLenEncInt(buffer);
      while (len > 0) {
        key = BufferHelper::getLenEncString(buffer);
        val = BufferHelper::getLenEncString(buffer);
        connAttribs_[key] = val;
        len -= (key.length() + val.length() + 1 + 1);
      }
    }
  } else {
    maxPktSize_ = BufferHelper::getInt24(buffer);
    userName_ = BufferHelper::getCString(buffer);
    password_ = BufferHelper::getStringFromRestOfBuffer(buffer);
  }
}

std::string ClientHandshakeMessage::toString() {
  std::stringstream s;

  s << "Max packet size: " << maxPktSize_ << std::endl;
  s << "Client language: " << collations[charset_] << std::endl;
  s << "username: " << userName_ << std::endl;
  s << "auth resp: " << authResp_ << std::endl;
  s << "db name: " << dbName_ << std::endl;
  s << "auth plugin name: " << authPluginName_ << std::endl;

  s << "Capabilities: " << capabilities_ << std::endl;
  s << "\t Client long password                 : "
    << ((capabilities_ & CLIENT_LONG_PASSWORD) ? string("SET") : string("NOT SET")) << std::endl;
  s << "\t Client found rows                    : "
    << ((capabilities_ & CLIENT_FOUND_ROWS) ? string("SET") : string("NOT SET")) << std::endl;
  s << "\t Client long flags                    : "
    << ((capabilities_ & CLIENT_LONG_FLAG) ? string("SET") : string("NOT SET")) << std::endl;
  s << "\t Client connect with DB               : "
    << ((capabilities_ & CLIENT_CONNECT_WITH_DB) ? string("SET") : string("NOT SET")) << std::endl;
  s << "\t Client no schema                     : "
    << ((capabilities_ & CLIENT_NO_SCHEMA) ? string("SET") : string("NOT SET")) << std::endl;
  s << "\t Client compress                      : "
    << ((capabilities_ & CLIENT_COMPRESS) ? string("SET") : string("NOT SET")) << std::endl;
  s << "\t Client ODBC                          : "
    << ((capabilities_ & CLIENT_ODBC) ? string("SET") : string("NOT SET")) << std::endl;
  s << "\t Client local files                   : "
    << ((capabilities_ & CLIENT_LOCAL_FILES) ? string("SET") : string("NOT SET")) << std::endl;
  s << "\t Client ignore space                  : "
    << ((capabilities_ & CLIENT_IGNORE_SPACE) ? string("SET") : string("NOT SET")) << std::endl;
  s << "\t Client protocol 41                   : "
    << ((capabilities_ & CLIENT_PROTOCOL_41) ? string("SET") : string("NOT SET")) << std::endl;
  s << "\t Client interactive                   : "
    << ((capabilities_ & CLIENT_INTERACTIVE) ? string("SET") : string("NOT SET")) << std::endl;
  s << "\t Client SSL                           : "
    << ((capabilities_ & CLIENT_SSL) ? string("SET") : string("NOT SET")) << std::endl;
  s << "\t Client ignore sigpipe                : "
    << ((capabilities_ & CLIENT_IGNORE_SIGPIPE) ? string("SET") : string("NOT SET")) << std::endl;
  s << "\t Client transactions                  : "
    << ((capabilities_ & CLIENT_TRANSACTIONS) ? string("SET") : string("NOT SET")) << std::endl;
  s << "\t Client reserved                      : "
    << ((capabilities_ & CLIENT_RESERVED) ? string("SET") : string("NOT SET")) << std::endl;
  s << "\t Client secure connection             : "
    << ((capabilities_ & CLIENT_SECURE_CONNECTION) ? string("SET") : string("NOT SET"))
    << std::endl;
  s << "\t Client multi statements              : "
    << ((capabilities_ & CLIENT_MULTI_STATEMENTS) ? string("SET") : string("NOT SET")) << std::endl;
  s << "\t Client multi results                 : "
    << ((capabilities_ & CLIENT_MULTI_RESULTS) ? string("SET") : string("NOT SET")) << std::endl;
  s << "\t Client PS multi results              : "
    << ((capabilities_ & CLIENT_PS_MULTI_RESULTS) ? string("SET") : string("NOT SET")) << std::endl;
  s << "\t Client plugin auth                   : "
    << ((capabilities_ & CLIENT_PLUGIN_AUTH) ? string("SET") : string("NOT SET")) << std::endl;
  s << "\t Client connect attrs                 : "
    << ((capabilities_ & CLIENT_CONNECT_ATTRS) ? string("SET") : string("NOT SET")) << std::endl;
  s << "\t Client plugin auth lenec data        : "
    << ((capabilities_ & CLIENT_PLUGIN_AUTH_LENENC_CLIENT_DATA) ? string("SET") : string("NOT SET"))
    << std::endl;
  s << "\t Client can handle expired passwords  : "
    << ((capabilities_ & CLIENT_CAN_HANDLE_EXPIRED_PASSWORDS) ? string("SET") : string("NOT SET"))
    << std::endl;
  s << "\t Client progress                      : "
    << ((capabilities_ & CLIENT_PROGRESS) ? string("SET") : string("NOT SET")) << std::endl;
  s << "\t Client SSL verify server cert        : "
    << ((capabilities_ & CLIENT_SSL_VERIFY_SERVER_CERT) ? string("SET") : string("NOT SET"))
    << std::endl;
  s << "\t Client remember options              : "
    << ((capabilities_ & CLIENT_REMEMBER_OPTIONS) ? string("SET") : string("NOT SET")) << std::endl;
  // s << "\t Client : " << (cap & ) ? "SET" : string("NOT SET") << std::endl;

  return s.str();
}

OkMessage::OkMessage() {}

void OkMessage::fromPacket(Packet& pkt) {
  Buffer::OwnedImpl& buffer = pkt.buffer_;

  uint8_t h = BufferHelper::getInt8(buffer);
  assert(h == 0x00);

  affectedRows_ = BufferHelper::getLenEncInt(buffer);
  lastInsertId_ = BufferHelper::getLenEncInt(buffer);
  if (pkt.capabilities_ & CLIENT_PROTOCOL_41) {
    status_ = BufferHelper::getInt16(buffer);
    warnings_ = BufferHelper::getInt16(buffer);
  } else if (pkt.capabilities_ & CLIENT_TRANSACTIONS) {
    status_ = BufferHelper::getInt16(buffer);
  }

  if (pkt.capabilities_ & CLIENT_SESSION_TRACKING) {
    info_ = BufferHelper::getLenEncString(buffer);
    if (status_ & SERVER_SESSION_STATE_CHANGED) {
      sessionStateChanges_ = BufferHelper::getLenEncString(buffer);
    }
  } else {
    info_ = BufferHelper::getStringFromRestOfBuffer(buffer);
  }
}

std::string OkMessage::toString() {
  std::stringstream s;

  s << "Affect Rows: " << affectedRows_;
  s << " Last insert id: " << lastInsertId_;
  s << " Status: " << status_;
  s << " Warnings: " << warnings_;
  s << " Info: " << info_;
  s << " Session state changes: " << sessionStateChanges_;

  return s.str();
}

ErrMessage::ErrMessage() {}

void ErrMessage::fromPacket(Packet& pkt) {
  Buffer::OwnedImpl& buffer = pkt.buffer_;

  // https://mariadb.com/kb/en/library/err_packet/

  uint8_t h = BufferHelper::getInt8(buffer);
  assert(h == 0xFF);

  errorCode_ = BufferHelper::getInt16(buffer);

  if (errorCode_ == 0xFFFF && pkt.capabilities_ & CLIENT_PROGRESS) {
    /* progress reporting */
    BufferHelper::skipBytes(buffer, 1); // Stage
    BufferHelper::skipBytes(buffer, 1); // Max stage
    BufferHelper::skipBytes(buffer, 3); // Progress
    errorMsg_ = BufferHelper::getLenEncString(buffer);
  } else {
    if (pkt.capabilities_ & CLIENT_PROTOCOL_41) {
      sqlMarker_ = BufferHelper::getStringFromBuffer(buffer, 1);
      sqlState_ = BufferHelper::getStringFromBuffer(buffer, 5);
    }
    errorMsg_ = BufferHelper::getStringFromRestOfBuffer(buffer);
  }
}

std::string ErrMessage::toString() {
  std::stringstream s;

  s << "Error Code: " << errorCode_;
  s << " Sql marker: " << sqlMarker_;
  s << " Sql state: " << sqlState_;
  s << " Error msg: " << errorMsg_;

  return s.str();
}

EofMessage::EofMessage() {}

void EofMessage::fromPacket(Packet& pkt) {
  Buffer::OwnedImpl& buffer = pkt.buffer_;

  uint8_t h = BufferHelper::getInt8(buffer);
  assert(h == 0xFE && buffer.length() < 9);

  if (pkt.capabilities_ & CLIENT_PROTOCOL_41) {
    warnings_ = BufferHelper::getInt16(buffer);
    status_ = BufferHelper::getInt16(buffer);
  }
}

std::string EofMessage::toString() {
  std::stringstream s;

  s << "Warnings: " << warnings_;
  s << " Status: " << status_ << std::endl;

  return s.str();
}

QueryMessage::QueryMessage() {}

void QueryMessage::fromPacket(Packet& pkt) {
  Buffer::Instance& buffer = pkt.buffer_;

  command_ = BufferHelper::getInt8(buffer);

  switch (command_) {
  case COM_SLEEP:
    commandName_ = "COM_SLEEP";
    break;
  case COM_QUIT:
    commandName_ = "COM_QUIT";
    break;
  case COM_INIT_DB:
    commandName_ = "COM_INIT_DB";
    break;
  case COM_QUERY:
    commandName_ = "COM_QUERY";
    info_ = BufferHelper::getStringFromRestOfBuffer(buffer);
    break;
  case COM_FIELD_LIST:
    commandName_ = "COM_FIELD_LIST";
    break;
  case COM_CREATE_DB:
    commandName_ = "COM_CREATE_DB";
    break;
  case COM_DROP_DB:
    commandName_ = "COM_DROP_DB";
    break;
  case COM_REFRESH:
    commandName_ = "COM_REFRESH";
    break;
  case COM_SHUTDOWN:
    commandName_ = "COM_SHUTDOWN";
    break;
  case COM_STATISTICS:
    commandName_ = "COM_STATISTICS";
    break;
  case COM_PROCESS_INFO:
    commandName_ = "COM_PROCESS_INFO";
    break;
  case COM_CONNECT:
    commandName_ = "COM_CONNECT";
    break;
  case COM_PROCESS_KILL:
    commandName_ = "COM_PROCESS_KILL";
    break;
  case COM_DEBUG:
    commandName_ = "COM_DEBUG";
    break;
  case COM_PING:
    commandName_ = "COM_PING";
    break;
  case COM_TIME:
    commandName_ = "COM_TIME";
    break;
  case COM_DELAYED_INSERT:
    commandName_ = "COM_DELAYED_INSERT";
    break;
  case COM_CHANGE_USER:
    commandName_ = "COM_CHANGE_USER";
    break;
  case COM_BINLOG_DUMP:
    commandName_ = "COM_BINLOG_DUMP";
    break;
  case COM_TABLE_DUMP:
    commandName_ = "COM_TABLE_DUMP";
    break;
  case COM_CONNECT_OUT:
    commandName_ = "COM_CONNECT_OUT";
    break;
  case COM_REGISTER_SLAVE:
    commandName_ = "COM_REGISTER_SLAVE";
    break;
  case COM_STMT_PREPARE:
    commandName_ = "COM_STMT_PREPARE";
    break;
  case COM_STMT_EXECUTE:
    commandName_ = "COM_STMT_EXECUTE";
    break;
  case COM_STMT_SEND_LONG_DATA:
    commandName_ = "COM_STMT_SEND_LONG_DATA";
    break;
  case COM_STMT_CLOSE:
    commandName_ = "COM_STMT_CLOSE";
    break;
  case COM_STMT_RESET:
    commandName_ = "COM_STMT_RESET";
    break;
  case COM_SET_OPTION:
    commandName_ = "COM_SET_OPTION";
    break;
  case COM_STMT_FETCH:
    commandName_ = "COM_STMT_FETCH";
    break;
  case COM_DAEMON:
    commandName_ = "COM_DAEMON";
    break;
  default:
    throw EnvoyException(fmt::format("Unknown command: {}", command_));
  }
}

std::string QueryMessage::toString() {
  std::stringstream s;

  s << "Command: " << std::to_string(command_) << " " << commandName_;
  s << " Info: " << info_ << std::endl;

  return s.str();
}


}; // namespace MySQL
