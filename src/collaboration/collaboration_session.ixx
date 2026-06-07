module;

#include <QDataStream>
#include <QHostAddress>
#include <QHostInfo>
#include <QHash>
#include <QNetworkInterface>
#include <QTcpServer>
#include <QTcpSocket>
#include <QObject>
#include <QByteArray>
#include <QVector>
#include <QString>
#include <QIODevice>

#include <mdns.h>
#include <nlohmann/json.hpp>

#include <array>
#include <atomic>
#include <chrono>
#include <cstring>
#include <thread>

export module CollaborationSession;

import std;
import Map;
import WorldUndoManager;
import CollaborationProtocol;
import CollaborationSnapshot;

struct MdnsServiceState;

export class CollaborationSession : public QObject {
	Q_OBJECT

  public:
	struct DiscoveredEndpoint {
		QString address;
		quint16 port = 0;
		QString name;
	};

  public:
	explicit CollaborationSession(Map* map, QObject* parent = nullptr);
	~CollaborationSession() override;

	bool host(quint16 port);
	bool join(const QString& address, quint16 port);
	void stop();
	void broadcast_world_command(const WorldCommand& command);
	static QVector<DiscoveredEndpoint> discover_lan_sessions();

  signals:
	void status_changed(const QString& status);
	void peer_count_changed(int peers);
	void snapshot_received();

  private:
	struct SocketState {
		QByteArray buffer;
		quint32 expected_size = 0;
		bool ready = false;
	};

	Map* map = nullptr;
	QTcpServer* server = nullptr;
	QTcpSocket* host_socket = nullptr;
	QVector<QTcpSocket*> peers;
	QHash<QTcpSocket*, SocketState> socket_state;
	std::unique_ptr<MdnsServiceState> mdns_service;
	bool is_host = false;

	void send_json(QTcpSocket* socket, const QString& message);
	void broadcast_json(const QString& message, QTcpSocket* exclude = nullptr);
	void process_socket(QTcpSocket* socket);
	void handle_message(QTcpSocket* sender, const QString& message);
};

constexpr const char* service_type = "_hivewe._tcp.local.";

struct MdnsServiceState {
	std::atomic_bool running {false};
	std::thread thread;
	std::vector<int> sockets;
	std::string hostname;
	std::string service_instance;
	std::string hostname_qualified;
	mdns_record_t record_ptr {};
	mdns_record_t record_srv {};
	mdns_record_t record_a {};
	mdns_record_t record_aaaa {};
	std::array<std::uint32_t, 512> buffer {};
};

struct DiscoveryState {
	QVector<CollaborationSession::DiscoveredEndpoint> endpoints;
	CollaborationSession::DiscoveredEndpoint current_endpoint;
};

namespace {

QByteArray framed_payload(const QByteArray& payload) {
	QByteArray frame;
	QDataStream stream(&frame, QIODevice::WriteOnly);
	stream.setByteOrder(QDataStream::BigEndian);
	stream << static_cast<quint32>(payload.size());
	stream.writeRawData(payload.constData(), payload.size());
	return frame;
}

nlohmann::json qstring_to_json(const QString& message) {
	return nlohmann::json::parse(message.toStdString());
}

WorldEditContext make_context(Map* map) {
	return WorldEditContext {
		.terrain = map->terrain,
		.units = map->units,
		.doodads = map->doodads,
		.brush = map->brush,
		.pathing_map = map->pathing_map,
	};
}

QHostAddress to_host_address(const struct sockaddr_in& address) {
	return QHostAddress(ntohl(address.sin_addr.s_addr));
}

QHostAddress to_host_address(const struct sockaddr_in6& address) {
	Q_IPV6ADDR ipv6 {};
	std::memcpy(ipv6.c, address.sin6_addr.s6_addr, sizeof(ipv6.c));
	return QHostAddress(ipv6);
}

QHostAddress primary_ipv4_address() {
	for (const auto& address : QNetworkInterface::allAddresses()) {
		if (address.protocol() == QHostAddress::IPv4Protocol && !address.isLoopback()) {
			return address;
		}
	}
	return QHostAddress::LocalHost;
}

QString sockaddr_to_string(const struct sockaddr* address) {
	if (!address) {
		return {};
	}
	if (address->sa_family == AF_INET) {
		return to_host_address(*reinterpret_cast<const struct sockaddr_in*>(address)).toString();
	}
	if (address->sa_family == AF_INET6) {
		return to_host_address(*reinterpret_cast<const struct sockaddr_in6*>(address)).toString();
	}
	return {};
}

std::vector<int> open_mdns_service_sockets() {
	std::vector<int> sockets;

	struct sockaddr_in ipv4_address {};
	ipv4_address.sin_family = AF_INET;
	ipv4_address.sin_addr.s_addr = INADDR_ANY;
	ipv4_address.sin_port = htons(MDNS_PORT);
	if (const int socket = mdns_socket_open_ipv4(&ipv4_address); socket >= 0) {
		sockets.push_back(socket);
	}

	struct sockaddr_in6 ipv6_address {};
	ipv6_address.sin6_family = AF_INET6;
	ipv6_address.sin6_addr = in6addr_any;
	ipv6_address.sin6_port = htons(MDNS_PORT);
	if (const int socket = mdns_socket_open_ipv6(&ipv6_address); socket >= 0) {
		sockets.push_back(socket);
	}

	return sockets;
}

std::vector<int> open_mdns_query_sockets() {
	std::vector<int> sockets;
	if (const int socket = mdns_socket_open_ipv4(nullptr); socket >= 0) {
		sockets.push_back(socket);
	}
	if (const int socket = mdns_socket_open_ipv6(nullptr); socket >= 0) {
		sockets.push_back(socket);
	}
	return sockets;
}

int mdns_service_callback(
	int socket,
	const struct sockaddr* from,
	size_t addrlen,
	mdns_entry_type_t entry,
	uint16_t query_id,
	uint16_t rtype,
	uint16_t rclass,
	uint32_t ttl,
	const void* data,
	size_t size,
	size_t name_offset,
	size_t name_length,
	size_t record_offset,
	size_t record_length,
	void* user_data
) {
	(void)socket;
	(void)query_id;
	(void)ttl;
	(void)record_offset;
	(void)name_length;

	auto* state = static_cast<MdnsServiceState*>(user_data);
	if (!state || (entry != MDNS_ENTRYTYPE_QUESTION)) {
		return 0;
	}

	char name_buffer[256] = {};
	size_t offset = name_offset;
	const mdns_string_t name = mdns_string_extract(data, size, &offset, name_buffer, sizeof(name_buffer));

	const std::string service_name = service_type;
	const std::string service_instance = state->service_instance;
	const std::string hostname_qualified = state->hostname_qualified;

	mdns_record_t answer {};
	mdns_record_t additional[3] = {};
	size_t additional_count = 0;

	auto send_answer = [&](mdns_record_t record) {
		uint16_t unicast = rclass & MDNS_UNICAST_RESPONSE;
		if (unicast) {
			mdns_query_answer_unicast(socket, from, addrlen, state->buffer.data(), state->buffer.size() * sizeof(std::uint32_t),
				query_id, static_cast<mdns_record_type_t>(rtype), name.str, name.length, record,
				nullptr, 0, additional, additional_count);
		} else {
			mdns_query_answer_multicast(socket, state->buffer.data(), state->buffer.size() * sizeof(std::uint32_t),
				record, nullptr, 0, additional, additional_count);
		}
	};

	if ((name.length == service_name.length()) && (std::strncmp(name.str, service_name.c_str(), name.length) == 0)) {
		if ((rtype == MDNS_RECORDTYPE_PTR) || (rtype == MDNS_RECORDTYPE_ANY)) {
			answer = state->record_ptr;
			additional[additional_count++] = state->record_srv;
			if (state->record_a.data.a.addr.sin_family == AF_INET) {
				additional[additional_count++] = state->record_a;
			}
			if (state->record_aaaa.data.aaaa.addr.sin6_family == AF_INET6) {
				additional[additional_count++] = state->record_aaaa;
			}
			send_answer(answer);
		}
	} else if ((name.length == service_instance.length()) && (std::strncmp(name.str, service_instance.c_str(), name.length) == 0)) {
		if ((rtype == MDNS_RECORDTYPE_SRV) || (rtype == MDNS_RECORDTYPE_ANY)) {
			answer = state->record_srv;
			if (state->record_a.data.a.addr.sin_family == AF_INET) {
				additional[additional_count++] = state->record_a;
			}
			if (state->record_aaaa.data.aaaa.addr.sin6_family == AF_INET6) {
				additional[additional_count++] = state->record_aaaa;
			}
			send_answer(answer);
		}
		} else if ((name.length == hostname_qualified.length()) && (std::strncmp(name.str, hostname_qualified.c_str(), name.length) == 0)) {
		if ((rtype == MDNS_RECORDTYPE_A) || (rtype == MDNS_RECORDTYPE_ANY)) {
			answer = state->record_a;
			additional[additional_count++] = state->record_aaaa;
			send_answer(answer);
		} else if ((rtype == MDNS_RECORDTYPE_AAAA) || (rtype == MDNS_RECORDTYPE_ANY)) {
			answer = state->record_aaaa;
			additional[additional_count++] = state->record_a;
			send_answer(answer);
		}
	}

	return 0;
}

int mdns_discovery_callback(
	int socket,
	const struct sockaddr* from,
	size_t addrlen,
	mdns_entry_type_t entry,
	uint16_t query_id,
	uint16_t rtype,
	uint16_t rclass,
	uint32_t ttl,
	const void* data,
	size_t size,
	size_t name_offset,
	size_t name_length,
	size_t record_offset,
	size_t record_length,
	void* user_data
) {
	(void)socket;
	(void)from;
	(void)addrlen;
	(void)query_id;
	(void)rclass;
	(void)ttl;
	(void)name_length;
	(void)record_offset;

	auto* state = static_cast<DiscoveryState*>(user_data);
	if (!state || (entry != MDNS_ENTRYTYPE_ANSWER && entry != MDNS_ENTRYTYPE_ADDITIONAL)) {
		return 0;
	}

	char name_buffer[256] = {};
	size_t offset = name_offset;
	const mdns_string_t name = mdns_string_extract(data, size, &offset, name_buffer, sizeof(name_buffer));
	(void)name;
	if ((rtype == MDNS_RECORDTYPE_PTR) && (record_length > 0)) {
		char target_buffer[256] = {};
		const mdns_string_t instance = mdns_record_parse_ptr(data, size, record_offset, record_length, target_buffer, sizeof(target_buffer));
		if (instance.length) {
			state->current_endpoint.name = QString::fromUtf8(instance.str, static_cast<int>(instance.length));
		}
	} else if ((rtype == MDNS_RECORDTYPE_SRV) && (record_length > 0)) {
		char target_buffer[256] = {};
		const mdns_record_srv_t srv = mdns_record_parse_srv(data, size, record_offset, record_length, target_buffer, sizeof(target_buffer));
		if (srv.port) {
			state->current_endpoint.port = srv.port;
		}
		if (srv.name.str && srv.name.length) {
			state->current_endpoint.name = QString::fromUtf8(srv.name.str, static_cast<int>(srv.name.length));
		}
	} else if ((rtype == MDNS_RECORDTYPE_A) && (record_length == 4)) {
		struct sockaddr_in address {};
		mdns_record_parse_a(data, size, record_offset, record_length, &address);
		state->current_endpoint.address = to_host_address(address).toString();
	} else if ((rtype == MDNS_RECORDTYPE_AAAA) && (record_length == 16)) {
		struct sockaddr_in6 address {};
		mdns_record_parse_aaaa(data, size, record_offset, record_length, &address);
		state->current_endpoint.address = to_host_address(address).toString();
	}

	if (state->current_endpoint.port && !state->current_endpoint.address.isEmpty() && !state->current_endpoint.name.isEmpty()) {
		const auto already_present = std::ranges::any_of(
			state->endpoints,
			[&](const CollaborationSession::DiscoveredEndpoint& endpoint) {
				return endpoint.address == state->current_endpoint.address && endpoint.port == state->current_endpoint.port;
			}
		);
		if (!already_present) {
			state->endpoints.push_back(state->current_endpoint);
		}
	}

	return 0;
}

void mdns_service_thread(MdnsServiceState* state) {
	auto buffer = std::array<std::uint32_t, 512> {};

	mdns_record_t additional[3] = {};
	size_t additional_count = 0;
	additional[additional_count++] = state->record_srv;
	if (state->record_a.data.a.addr.sin_family == AF_INET) {
		additional[additional_count++] = state->record_a;
	}
	if (state->record_aaaa.data.aaaa.addr.sin6_family == AF_INET6) {
		additional[additional_count++] = state->record_aaaa;
	}

	for (const int socket : state->sockets) {
		mdns_announce_multicast(socket, buffer.data(), buffer.size() * sizeof(std::uint32_t), state->record_ptr, nullptr, 0,
			additional, additional_count);
	}

	while (state->running.load()) {
		fd_set readfs;
		FD_ZERO(&readfs);
		int nfds = 0;
		for (const int socket : state->sockets) {
			FD_SET(socket, &readfs);
			if (socket >= nfds) {
				nfds = socket + 1;
			}
		}

		timeval timeout {};
		timeout.tv_usec = 100000;
		if (select(nfds, &readfs, nullptr, nullptr, &timeout) < 0) {
			break;
		}

		for (const int socket : state->sockets) {
			if (FD_ISSET(socket, &readfs)) {
				mdns_socket_listen(socket, buffer.data(), buffer.size() * sizeof(std::uint32_t), mdns_service_callback, state);
			}
		}
	}

	for (const int socket : state->sockets) {
		mdns_socket_close(socket);
	}
	state->sockets.clear();
}

}

CollaborationSession::CollaborationSession(Map* map, QObject* parent)
	: QObject(parent), map(map), server(new QTcpServer(this)) {
	connect(server, &QTcpServer::newConnection, this, [this]() {
		while (server->hasPendingConnections()) {
			auto* client = server->nextPendingConnection();
			peers.push_back(client);
			auto& state = socket_state[client];
			state = {};
			state.ready = false;
			connect(client, &QTcpSocket::readyRead, this, [this, client]() { process_socket(client); });
			connect(client, &QTcpSocket::disconnected, this, [this, client]() {
				peers.removeOne(client);
				socket_state.remove(client);
				client->deleteLater();
				emit peer_count_changed(peers.size());
			});
			emit peer_count_changed(peers.size());
			emit status_changed(QString::fromStdString(std::format("Peer connected ({})", peers.size())));
		}
	});
}

CollaborationSession::~CollaborationSession() {
	stop();
}

bool CollaborationSession::host(const quint16 port) {
	stop();
	is_host = true;
	if (!server->listen(QHostAddress::AnyIPv4, port)) {
		emit status_changed(QString::fromStdString(std::format("Failed to listen on port {}", port)));
		return false;
	}

	mdns_service = std::make_unique<MdnsServiceState>();
	mdns_service->hostname = QHostInfo::localHostName().toStdString();
	if (mdns_service->hostname.empty()) {
		mdns_service->hostname = "HiveWE";
	}
	mdns_service->service_instance = mdns_service->hostname + "." + service_type;
	mdns_service->hostname_qualified = mdns_service->hostname + ".local.";

	const auto host_address = primary_ipv4_address();
	mdns_service->record_ptr = {};
	mdns_service->record_ptr.name = {service_type, std::strlen(service_type)};
	mdns_service->record_ptr.type = MDNS_RECORDTYPE_PTR;
	mdns_service->record_ptr.data.ptr.name = {mdns_service->service_instance.c_str(), mdns_service->service_instance.size()};
	mdns_service->record_ptr.rclass = 0;
	mdns_service->record_ptr.ttl = 0;

	mdns_service->record_srv = {};
	mdns_service->record_srv.name = {mdns_service->service_instance.c_str(), mdns_service->service_instance.size()};
	mdns_service->record_srv.type = MDNS_RECORDTYPE_SRV;
	mdns_service->record_srv.data.srv.priority = 0;
	mdns_service->record_srv.data.srv.weight = 0;
	mdns_service->record_srv.data.srv.port = port;
	mdns_service->record_srv.data.srv.name = {mdns_service->hostname_qualified.c_str(), mdns_service->hostname_qualified.size()};
	mdns_service->record_srv.rclass = 0;
	mdns_service->record_srv.ttl = 0;

	mdns_service->record_a = {};
	mdns_service->record_a.name = {mdns_service->hostname_qualified.c_str(), mdns_service->hostname_qualified.size()};
	mdns_service->record_a.type = MDNS_RECORDTYPE_A;
	mdns_service->record_a.data.a.addr.sin_family = AF_INET;
	mdns_service->record_a.rclass = 0;
	mdns_service->record_a.ttl = 0;
	mdns_service->record_a.data.a.addr.sin_addr.s_addr = htonl(host_address.toIPv4Address());

	mdns_service->record_aaaa = {};
	mdns_service->record_aaaa.name = {mdns_service->hostname_qualified.c_str(), mdns_service->hostname_qualified.size()};
	mdns_service->record_aaaa.type = MDNS_RECORDTYPE_AAAA;
	mdns_service->record_aaaa.rclass = 0;
	mdns_service->record_aaaa.ttl = 0;
	mdns_service->record_aaaa.data.aaaa.addr.sin6_family = AF_INET6;

	mdns_service->sockets = open_mdns_service_sockets();
	if (mdns_service->sockets.empty()) {
		mdns_service.reset();
		emit status_changed("Failed to open mDNS sockets");
		return false;
	}
	mdns_service->running = true;
	mdns_service->thread = std::thread(mdns_service_thread, mdns_service.get());
	if (map) {
		map->world_undo.on_action_added = [this](const WorldCommand& command) {
			broadcast_world_command(command);
		};
	}
	emit status_changed(QString::fromStdString(std::format("Hosting on port {}", port)));
	return true;
}

bool CollaborationSession::join(const QString& address, const quint16 port) {
	stop();
	is_host = false;
	host_socket = new QTcpSocket(this);
	socket_state.insert(host_socket, {});
	connect(host_socket, &QTcpSocket::readyRead, this, [this]() { process_socket(host_socket); });
	connect(host_socket, &QTcpSocket::disconnected, this, [this]() {
		if (host_socket) {
			host_socket->deleteLater();
			host_socket = nullptr;
		}
		if (map) {
			map->world_undo.on_action_added = {};
		}
		emit status_changed("Disconnected from host");
	});
	host_socket->connectToHost(address, port);
	if (!host_socket->waitForConnected(3000)) {
		const QString error = host_socket->errorString();
		host_socket->deleteLater();
		host_socket = nullptr;
		emit status_changed(QString::fromStdString(std::format("Failed to connect: {}", error.toStdString())));
		return false;
	}
	if (map) {
		map->world_undo.on_action_added = [this](const WorldCommand& command) {
			broadcast_world_command(command);
		};
	}
	send_json(host_socket, QStringLiteral(R"({"type":"join_request"})"));
	emit status_changed(QString::fromStdString(std::format("Connected to {}:{}", address.toStdString(), port)));
	return true;
}

void CollaborationSession::stop() {
	if (mdns_service) {
		mdns_service->running = false;
		if (mdns_service->thread.joinable()) {
			mdns_service->thread.join();
		}
		mdns_service.reset();
	}

	if (map) {
		map->world_undo.on_action_added = {};
	}

	for (auto* peer : peers) {
		peer->disconnect(this);
		peer->deleteLater();
	}
	peers.clear();
	socket_state.clear();

	if (host_socket) {
		host_socket->disconnect(this);
		host_socket->deleteLater();
		host_socket = nullptr;
	}

	if (server->isListening()) {
		server->close();
	}
	is_host = false;
	emit peer_count_changed(0);
}

void CollaborationSession::broadcast_world_command(const WorldCommand& command) {
	const nlohmann::json message = nlohmann::json{{"type", "world_command"}, {"command", collaboration::serialize_world_command(command)}};
	const QByteArray frame = framed_payload(QByteArray::fromStdString(message.dump()));

	if (is_host) {
		for (auto* peer : peers) {
			if (socket_state.value(peer).ready) {
				peer->write(frame);
			}
		}
	} else if (host_socket) {
		host_socket->write(frame);
	}
}

void CollaborationSession::send_json(QTcpSocket* socket, const QString& message) {
	if (!socket) {
		return;
	}
	socket->write(framed_payload(message.toUtf8()));
}

void CollaborationSession::broadcast_json(const QString& message, QTcpSocket* exclude) {
	const QByteArray frame = framed_payload(message.toUtf8());
	if (is_host) {
		for (auto* peer : peers) {
			if ((peer != exclude) && socket_state.value(peer).ready) {
				peer->write(frame);
			}
		}
	} else if (host_socket && host_socket != exclude) {
		host_socket->write(frame);
	}
}

QVector<CollaborationSession::DiscoveredEndpoint> CollaborationSession::discover_lan_sessions() {
	DiscoveryState state;
	auto sockets = open_mdns_query_sockets();
	if (sockets.empty()) {
		return state.endpoints;
	}

	auto buffer = std::array<std::uint32_t, 512> {};
	std::vector<int> query_ids;
	query_ids.reserve(sockets.size());
	for (const int socket : sockets) {
		query_ids.push_back(mdns_query_send(socket, MDNS_RECORDTYPE_PTR, service_type, std::strlen(service_type), buffer.data(), buffer.size() * sizeof(std::uint32_t), 0));
	}

	const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
	while (std::chrono::steady_clock::now() < deadline) {
		fd_set readfs;
		FD_ZERO(&readfs);
		int nfds = 0;
		for (const int socket : sockets) {
			FD_SET(socket, &readfs);
			if (socket >= nfds) {
				nfds = socket + 1;
			}
		}

		timeval timeout {};
		timeout.tv_usec = 200000;
		if (select(nfds, &readfs, nullptr, nullptr, &timeout) <= 0) {
			continue;
		}

		for (size_t index = 0; index < sockets.size(); ++index) {
			if (FD_ISSET(sockets[index], &readfs)) {
				mdns_query_recv(
					sockets[index],
					buffer.data(),
					buffer.size() * sizeof(std::uint32_t),
					mdns_discovery_callback,
					&state,
					query_ids[index]
				);
			}
		}
	}

	for (const int socket : sockets) {
		mdns_socket_close(socket);
	}

	return state.endpoints;
}

void CollaborationSession::process_socket(QTcpSocket* socket) {
	if (!socket_state.contains(socket)) {
		socket_state.insert(socket, {});
	}
	auto& state = socket_state[socket];
	state.buffer.append(socket->readAll());

	while (true) {
		if (state.expected_size == 0) {
			if (state.buffer.size() < static_cast<int>(sizeof(quint32))) {
				return;
			}
			QDataStream stream(state.buffer.left(sizeof(quint32)));
			stream.setByteOrder(QDataStream::BigEndian);
			stream >> state.expected_size;
			state.buffer.remove(0, sizeof(quint32));
		}

		if (state.buffer.size() < static_cast<int>(state.expected_size)) {
			return;
		}

		const QByteArray payload = state.buffer.left(state.expected_size);
		state.buffer.remove(0, state.expected_size);
		state.expected_size = 0;
		handle_message(socket, QString::fromUtf8(payload));
	}
}

void CollaborationSession::handle_message(QTcpSocket* sender, const QString& message) {
	const auto data = qstring_to_json(message);
	const auto type = data.value("type", "");

	if (type == "join_request" && is_host) {
		if (!map) {
			return;
		}
		const auto snapshot = nlohmann::json{{"type", "snapshot"}, {"map", collaboration::serialize_map_snapshot(*map)}};
		send_json(sender, QString::fromStdString(snapshot.dump()));
		socket_state[sender].ready = true;
		return;
	}

	if (type == "snapshot") {
		if (!map) {
			return;
		}
		collaboration::deserialize_map_snapshot(data.at("map"), *map);
		if (sender == host_socket) {
			socket_state[host_socket].ready = true;
		}
		emit snapshot_received();
		return;
	}

	if (type != "world_command") {
		return;
	}

	if (!map) {
		return;
	}

	auto ctx = make_context(map);
	auto command = collaboration::deserialize_world_command(data.at("command"), ctx);
	map->world_undo.apply_remote(std::move(command), ctx);

	if (is_host) {
		broadcast_json(message, sender);
	}
}

#include "collaboration_session.moc"