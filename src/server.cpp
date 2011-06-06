// server.cpp
//
#include "server.h"
#include "player.h"
#include "events.h"
#include "network/response.h"

#include <boost/foreach.hpp>
#define foreach BOOST_FOREACH

#include <iostream>

namespace boostcraft {

using boost::asio::ip::tcp;

Server::Server(boost::asio::io_service& io, tcp::endpoint& endpoint)
    : TcpServer(io, endpoint)
{
    // Register event handlers
    // TODO: events should be rewritten to use std::function
    //
    boost::function<void(PlayerDisconnectEvent&)> disconnect =
        boost::bind(&Server::onPlayerDisconnect, this, _1);

    boost::function<void(ChatEvent&)> chat = 
        boost::bind(&Server::onChat, this, _1);

    listen(disconnect);
    listen(chat);

    //
    // TODO: "listen" needs to return a signal connection so we can remove the
    //       listener when this object is destroyed... Granted, it's less of an
    //       issue with Server, since there is one Server instance that doesn't
    //       go out of scope until the io_service has already ceased running,
    //       but other use cases may require it.
    //
}

void Server::connect(std::unique_ptr<tcp::socket> socket)
{
    // Create a player from the socket
    std::shared_ptr<Player> player(new Player(std::move(socket)));
    players.push_back(player);
    player->start();

    // TODO: it's not safe to call start() on a player until after a shared_ptr
    // owning the player has been created!!! this should probably be addressed
    // by forcing the use of a static Player::create function.
}

void Server::onPlayerDisconnect(PlayerDisconnectEvent& e)
{
    players.remove(e.player);

    auto qmsg = network::chatmessage("§5* " + e.player->name() + " has left the server.");
    foreach(auto player, players)
        player->deliver(qmsg);
}

void Server::onChat(ChatEvent& e)
{
    // Relay the event to all players
    auto msg = network::chatmessage("<" + e.player->name() + "> " + e.message);
    foreach(auto player, players)
        player->deliver(msg);
}

} // namespace boostcraft

