#include "tcpserver.h"
//#include "debug/datalog.h"

#include <boost/lexical_cast.hpp>

namespace network
{

TcpServer::TcpServer() : isInit_(false), lastPacketForSending_( "")
{
	bufferForReadPtr_ = BufferForReadPtr( new BufferForRead);
}

TcpServer::~TcpServer()
{
	if( acceptorPtr_.get())
		acceptorPtr_->close();

	if( socketPtr_.get())
		socketPtr_->close();

	service_.reset();
	ioServiceThread_.interrupt();
	ioServiceThread_.join();
}

void TcpServer::setHandlerTcpPacket( HandlePacket handlePacket)
{
	handlePacket_ = handlePacket;
}

bool TcpServer::init( uint16_t port)
{
	try
	{
		endpoint_ = boost::asio::ip::tcp::endpoint( boost::asio::ip::tcp::v4(), port);
		expectConnection();

		isInit_ = true;

		return true;
	}
	catch( boost::system::system_error& error)
	{
		std::cout << error.what() << std::endl;
		return false;
	}
}

void TcpServer::expectConnection()
{
	try
	{
		if( acceptorPtr_.get())
			acceptorPtr_->close();

		if( socketPtr_.get())
			socketPtr_->close();

		acceptorPtr_ = AcceptorPtr( new boost::asio::ip::tcp::acceptor( service_, endpoint_));
		socketPtr_ = SocketPtr( new boost::asio::ip::tcp::socket( service_));
		acceptorPtr_->listen();

		acceptorPtr_->async_accept( *socketPtr_, boost::bind( &TcpServer::startReading, this));
		ioServiceThread_ = boost::thread( boost::bind( &boost::asio::io_service::run, &service_));
	}
	catch( std::exception& e)
	{
		std::cout << e.what() << std::endl;
	}
}

void TcpServer::startReading()
{
	if( !socketPtr_.get() || !socketPtr_->is_open())
		return;

	// Как только запускается клиент, отправляем ему последний отправленный пакет.
	// Т.е. когда запустится эмулятор (клиент), то ему сразу придет пакет с последним содержимым экрана.

	sendPacket( lastPacketForSending_);

	socketPtr_->async_read_some( boost::asio::buffer( *bufferForReadPtr_),
		boost::bind( &TcpServer::handleReading, this, bufferForReadPtr_, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
}

void TcpServer::handleReading( BufferForReadPtr bufferPtr, const boost::system::error_code& error, size_t bytes_transferred)
{
	if( !socketPtr_.get() || !socketPtr_->is_open() || error.value() != 0)
	{
		expectConnection();
		return;
	}

	std::string packet;
	std::copy( bufferPtr->begin(), bufferPtr->begin()+bytes_transferred, std::back_inserter( packet));

	handlePacket_( packet);

	socketPtr_->async_read_some( boost::asio::buffer( *bufferForReadPtr_),
		boost::bind( &TcpServer::handleReading, this, bufferForReadPtr_, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
}

void TcpServer::sendPacket( const std::string& packet)
{
	lastPacketForSending_ = packet;

	if( !isInit_)
		return;

	if( !socketPtr_.get() || !socketPtr_->is_open())
		return;

	try
	{
		boost::asio::async_write( *socketPtr_, boost::asio::buffer( packet),
			boost::bind( &TcpServer::handleWriting, this));
	}
	catch( const std::exception& e)
	{
		std::cout << e.what() << std::endl;
	}
}

void TcpServer::handleWriting()
{

}

} /* namespace application */