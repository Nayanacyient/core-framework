/*
 * This file is protected by Copyright. Please refer to the COPYRIGHT file
 * distributed with this source distribution.
 *
 * This file is part of REDHAWK bulkioInterfaces.
 *
 * REDHAWK bulkioInterfaces is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version.
 *
 * REDHAWK bulkioInterfaces is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see http://www.gnu.org/licenses/.
 */

#include "InPortTest.h"

#include <boost/scoped_ptr.hpp>

class SriListener {
public:
    void updateSRI(BULKIO::StreamSRI& sri)
    {
        this->sri.push_back(sri);
    }

    std::vector<BULKIO::StreamSRI> sri;
};

template <class Port>
void InPortTest<Port>::testLegacyAPI()
{
    // Test for methods that are technically still supported, but discouraged
    port->enableStats(false);

    port->block();

    port->unblock();
}

template <class Port>
void InPortTest<Port>::testGetPacket()
{
    // Port queue starts empty
    boost::scoped_ptr<PacketType> packet;
    packet.reset(port->getPacket(bulkio::Const::NON_BLOCKING));
    CPPUNIT_ASSERT(!packet);

    const char* stream_id = "test_get_packet";
    BULKIO::StreamSRI sri = bulkio::sri::create(stream_id);
    port->pushSRI(sri);

    BULKIO::PrecisionUTCTime ts = bulkio::time::utils::now();
    this->_pushTestPacket(50, ts, false, stream_id);

    // Check result of getPacket
    packet.reset(port->getPacket(bulkio::Const::NON_BLOCKING));
    CPPUNIT_ASSERT(packet);
    CPPUNIT_ASSERT_EQUAL((size_t) 50, packet->dataBuffer.size());
    CPPUNIT_ASSERT(!packet->EOS);
    CPPUNIT_ASSERT_EQUAL(std::string(stream_id), packet->streamID);
    CPPUNIT_ASSERT(bulkio::sri::DefaultComparator(sri, packet->SRI));
    CPPUNIT_ASSERT(packet->sriChanged);
    CPPUNIT_ASSERT(!packet->inputQueueFlushed);

    // No packet, should return null
    packet.reset(port->getPacket(bulkio::Const::NON_BLOCKING));
    CPPUNIT_ASSERT(!packet);

    // Change mode to complex and push another packet with EOS set
    sri.mode = 1;
    port->pushSRI(sri);
    this->_pushTestPacket(100, ts, true, sri.streamID);
    packet.reset(port->getPacket(bulkio::Const::NON_BLOCKING));
    CPPUNIT_ASSERT(packet);
    CPPUNIT_ASSERT_EQUAL((size_t) 100, packet->dataBuffer.size());
    CPPUNIT_ASSERT(packet->EOS);
    CPPUNIT_ASSERT(packet->sriChanged);
    CPPUNIT_ASSERT_EQUAL(1, (int) packet->SRI.mode);
}

template <class Port>
void InPortTest<Port>::testGetPacketStreamRemoved()
{
    typedef typename Port::StreamList StreamList;

    // Create a new stream and push some data to it
    const char* stream_id = "test_get_packet_stream_removed";
    BULKIO::StreamSRI sri = bulkio::sri::create(stream_id);
    port->pushSRI(sri);

    this->_pushTestPacket(50, bulkio::time::utils::now(), true, stream_id);

    StreamList streams = port->getStreams();
    CPPUNIT_ASSERT_EQUAL((size_t) 1, streams.size());

    boost::scoped_ptr<PacketType> packet;
    packet.reset(port->getPacket(bulkio::Const::NON_BLOCKING));
    CPPUNIT_ASSERT(packet);
    CPPUNIT_ASSERT_EQUAL(true, packet->EOS);

    // The set of streams should be empty again
    streams = port->getStreams();
    CPPUNIT_ASSERT(streams.empty());
}

template <class Port>
void InPortTest<Port>::testActiveSRIs()
{
    BULKIO::StreamSRISequence_var active_sris = port->activeSRIs();
    CPPUNIT_ASSERT_EQUAL((CORBA::ULong) 0, active_sris->length());

    // Push a new SRI, and make sure that it is immediately visible and
    // correct in activeSRIs
    const char* stream_id_1 = "active_sri_1";
    BULKIO::StreamSRI sri_1 = bulkio::sri::create(stream_id_1);
    port->pushSRI(sri_1);
    active_sris = port->activeSRIs();
    CPPUNIT_ASSERT_EQUAL((CORBA::ULong) 1, active_sris->length());
    CPPUNIT_ASSERT(bulkio::sri::DefaultComparator(active_sris[0], sri_1));

    // Push a second SRI, and make sure that activeSRIs is up-to-date
    const char* stream_id_2 = "active_sri_2";
    BULKIO::StreamSRI sri_2 = bulkio::sri::create(stream_id_2);
    port->pushSRI(sri_2);
    active_sris = port->activeSRIs();
    CPPUNIT_ASSERT_EQUAL((CORBA::ULong) 2, active_sris->length());
    for (size_t index = 0; index < active_sris->length(); ++index) {
        const std::string current_id(active_sris[index].streamID);
        if (current_id == stream_id_2) {
            CPPUNIT_ASSERT(bulkio::sri::DefaultComparator(active_sris[index], sri_2));
        } else if (current_id != stream_id_1) {
            CPPUNIT_FAIL("unexpected SRI '" + current_id + "'");
        }
    }

    // Push an end-of-stream, retrieve the packet, and verify that the
    // stream is no longer in activeSRIs
    this->_pushTestPacket(0, bulkio::time::utils::notSet(), true, stream_id_1);
    boost::scoped_ptr<PacketType> packet;
    packet.reset(port->getPacket(bulkio::Const::NON_BLOCKING));
    CPPUNIT_ASSERT(packet);
    CPPUNIT_ASSERT(packet->EOS);
    active_sris = port->activeSRIs();
    CPPUNIT_ASSERT_EQUAL((CORBA::ULong) 1, active_sris->length());
    CPPUNIT_ASSERT_EQUAL(std::string(stream_id_2), std::string(active_sris[0].streamID));
}

template <class Port>
void InPortTest<Port>::testStreamIds()
{
    boost::scoped_ptr<PacketType> packet;
    packet.reset(port->getPacket(bulkio::Const::NON_BLOCKING));
    CPPUNIT_ASSERT(!packet);

    const char* stream_id = "test_get_packet";
    BULKIO::StreamSRI sri = bulkio::sri::create(stream_id);
    sri.mode = 0;
    port->pushSRI(sri);

    this->_pushTestPacket(50, bulkio::time::utils::now(), false, stream_id);
    this->_pushTestPacket(50, bulkio::time::utils::now(), false, stream_id);
    this->_pushTestPacket(50, bulkio::time::utils::now(), true, stream_id);

    sri.mode = 1;
    port->pushSRI(sri);

    this->_pushTestPacket(50, bulkio::time::utils::now(), false, stream_id);
    this->_pushTestPacket(50, bulkio::time::utils::now(), false, stream_id);
    this->_pushTestPacket(50, bulkio::time::utils::now(), true, stream_id);

    // Check result of getPacket
    packet.reset(port->getPacket(bulkio::Const::NON_BLOCKING));
    CPPUNIT_ASSERT(packet);
    CPPUNIT_ASSERT_EQUAL((size_t) 50, packet->dataBuffer.size());
    CPPUNIT_ASSERT(!packet->EOS);
    CPPUNIT_ASSERT_EQUAL(0, (int) packet->SRI.mode);

    packet.reset(port->getPacket(bulkio::Const::NON_BLOCKING));
    CPPUNIT_ASSERT(packet);
    CPPUNIT_ASSERT_EQUAL((size_t) 50, packet->dataBuffer.size());
    CPPUNIT_ASSERT(!packet->EOS);
    CPPUNIT_ASSERT_EQUAL(0, (int) packet->SRI.mode);

    packet.reset(port->getPacket(bulkio::Const::NON_BLOCKING));
    CPPUNIT_ASSERT(packet);
    CPPUNIT_ASSERT_EQUAL((size_t) 50, packet->dataBuffer.size());
    CPPUNIT_ASSERT(packet->EOS);
    CPPUNIT_ASSERT_EQUAL(0, (int) packet->SRI.mode);

    packet.reset(port->getPacket(bulkio::Const::NON_BLOCKING));
    CPPUNIT_ASSERT(packet);
    CPPUNIT_ASSERT_EQUAL((size_t) 50, packet->dataBuffer.size());
    CPPUNIT_ASSERT(!packet->EOS);
    CPPUNIT_ASSERT_EQUAL(1, (int) packet->SRI.mode);

    packet.reset(port->getPacket(bulkio::Const::NON_BLOCKING));
    CPPUNIT_ASSERT(packet);
    CPPUNIT_ASSERT_EQUAL((size_t) 50, packet->dataBuffer.size());
    CPPUNIT_ASSERT(!packet->EOS);
    CPPUNIT_ASSERT_EQUAL(1, (int) packet->SRI.mode);

    packet.reset(port->getPacket(bulkio::Const::NON_BLOCKING));
    CPPUNIT_ASSERT(packet);
    CPPUNIT_ASSERT_EQUAL((size_t) 50, packet->dataBuffer.size());
    CPPUNIT_ASSERT(packet->EOS);
    CPPUNIT_ASSERT_EQUAL(1, (int) packet->SRI.mode);

    sri.mode = 0;
    port->pushSRI(sri);

    this->_pushTestPacket(100, bulkio::time::utils::now(), false, stream_id);
    this->_pushTestPacket(100, bulkio::time::utils::now(), false, stream_id);
    this->_pushTestPacket(100, bulkio::time::utils::now(), true, stream_id);
    sri.mode = 1;
    port->pushSRI(sri);

    this->_pushTestPacket(100, bulkio::time::utils::now(), false, stream_id);
    packet.reset(port->getPacket(bulkio::Const::NON_BLOCKING));

    CPPUNIT_ASSERT(packet);
    CPPUNIT_ASSERT_EQUAL((size_t) 100, packet->dataBuffer.size());
    CPPUNIT_ASSERT(!packet->EOS);
    CPPUNIT_ASSERT_EQUAL(0, (int) packet->SRI.mode);

    packet.reset(port->getPacket(bulkio::Const::NON_BLOCKING));
    CPPUNIT_ASSERT(packet);
    CPPUNIT_ASSERT_EQUAL((size_t) 100, packet->dataBuffer.size());
    CPPUNIT_ASSERT(!packet->EOS);
    CPPUNIT_ASSERT_EQUAL(0, (int) packet->SRI.mode);

    packet.reset(port->getPacket(bulkio::Const::NON_BLOCKING));
    CPPUNIT_ASSERT(packet);
    CPPUNIT_ASSERT_EQUAL((size_t) 100, packet->dataBuffer.size());
    CPPUNIT_ASSERT(packet->EOS);
    CPPUNIT_ASSERT_EQUAL(0, (int) packet->SRI.mode);

    packet.reset(port->getPacket(bulkio::Const::NON_BLOCKING));
    CPPUNIT_ASSERT(packet);
    CPPUNIT_ASSERT_EQUAL((size_t) 100, packet->dataBuffer.size());
    CPPUNIT_ASSERT(!packet->EOS);
    CPPUNIT_ASSERT_EQUAL(1, (int) packet->SRI.mode);

    this->_pushTestPacket(100, bulkio::time::utils::now(), false, stream_id);
    this->_pushTestPacket(100, bulkio::time::utils::now(), true, stream_id);

    packet.reset(port->getPacket(bulkio::Const::NON_BLOCKING));
    CPPUNIT_ASSERT(packet);
    CPPUNIT_ASSERT_EQUAL((size_t) 100, packet->dataBuffer.size());
    CPPUNIT_ASSERT(!packet->EOS);
    CPPUNIT_ASSERT_EQUAL(1, (int) packet->SRI.mode);

    packet.reset(port->getPacket(bulkio::Const::NON_BLOCKING));
    CPPUNIT_ASSERT(packet);
    CPPUNIT_ASSERT_EQUAL((size_t) 100, packet->dataBuffer.size());
    CPPUNIT_ASSERT(packet->EOS);
    CPPUNIT_ASSERT_EQUAL(1, (int) packet->SRI.mode);
}

template <class Port>
void InPortTest<Port>::testQueueDepth()
{
    const char* stream_id = "test_state";

    // The port had better start with an empty queue
    CPPUNIT_ASSERT_EQUAL(0, port->getCurrentQueueDepth());

    port->setMaxQueueDepth(4);

    // Use a non-blocking stream to allow queue flushing
    BULKIO::StreamSRI sri = bulkio::sri::create(stream_id);
    sri.blocking = false;
    port->pushSRI(sri);

    // Push some test packets, the queue should start growing
    for (int ii = 0; ii < 4; ii++) {
        this->_pushTestPacket(ii+1, bulkio::time::utils::now(), false, stream_id);
    }
    CPPUNIT_ASSERT_EQUAL(4, port->getCurrentQueueDepth());

    // Read a packet and make sure the current depth drops
    boost::scoped_ptr<PacketType> packet;
    packet.reset(port->getPacket(bulkio::Const::NON_BLOCKING));
    CPPUNIT_ASSERT(packet);
    CPPUNIT_ASSERT_EQUAL(3, port->getCurrentQueueDepth());

    // Reduce the max queue size and push another packet, causing a flush
    port->setMaxQueueDepth(3);
    this->_pushTestPacket(5, bulkio::time::utils::now(), false, stream_id);
    CPPUNIT_ASSERT_EQUAL(1, port->getCurrentQueueDepth());

    // Read the packet and make sure the flush is reported
    packet.reset(port->getPacket(bulkio::Const::NON_BLOCKING));
    CPPUNIT_ASSERT(packet);
    CPPUNIT_ASSERT(packet->inputQueueFlushed);
    CPPUNIT_ASSERT(not packet->EOS);
    CPPUNIT_ASSERT(not packet->sriChanged);
    CPPUNIT_ASSERT_EQUAL((size_t)5, packet->dataBuffer.size());

    // One more packet, should not report a flush
    this->_pushTestPacket(6, bulkio::time::utils::now(), false, stream_id);
    packet.reset(port->getPacket(bulkio::Const::NON_BLOCKING));
    CPPUNIT_ASSERT(packet);
    CPPUNIT_ASSERT(not packet->inputQueueFlushed);
    CPPUNIT_ASSERT_EQUAL((size_t)6, packet->dataBuffer.size());
}

template <class Port>
void InPortTest<Port>::testState()
{
    const char* stream_id = "test_state";

    // Port starts out idle
    CPPUNIT_ASSERT_EQUAL(BULKIO::IDLE, port->state());

    // Push one test packet, state goes to active
    BULKIO::StreamSRI sri = bulkio::sri::create(stream_id);
    port->pushSRI(sri);
    this->_pushTestPacket(1, bulkio::time::utils::now(), false, stream_id);
    CPPUNIT_ASSERT_EQUAL(BULKIO::ACTIVE, port->state());

    // Full queue should report busy
    port->setMaxQueueDepth(2);
    this->_pushTestPacket(1, bulkio::time::utils::now(), false, stream_id);
    CPPUNIT_ASSERT_EQUAL(BULKIO::BUSY, port->state());

    // Drop below max, back to active
    boost::scoped_ptr<PacketType> packet;
    packet.reset(port->getPacket(bulkio::Const::NON_BLOCKING));
    CPPUNIT_ASSERT(packet);
    CPPUNIT_ASSERT_EQUAL(BULKIO::ACTIVE, port->state());

    // Empty queue, back to idle
    packet.reset(port->getPacket(bulkio::Const::NON_BLOCKING));
    CPPUNIT_ASSERT(packet);
    CPPUNIT_ASSERT_EQUAL(BULKIO::IDLE, port->state());
}

template <class Port>
void InPortTest<Port>::testSriChanged()
{
    const char* stream_id = "sri_changed";

    SriListener listener;
    port->setNewStreamListener(&listener, &SriListener::updateSRI);
    CPPUNIT_ASSERT(listener.sri.empty());

    // Create a default SRI and push it, which should trigger the callback
    BULKIO::StreamSRI sri = bulkio::sri::create(stream_id);
    port->pushSRI(sri);
    CPPUNIT_ASSERT_EQUAL((size_t) 1, listener.sri.size());
    CPPUNIT_ASSERT_EQUAL(std::string(stream_id), std::string(listener.sri.back().streamID));

    // SRI should report changed for first packet
    this->_pushTestPacket(1, bulkio::time::utils::now(), false, stream_id);
    boost::scoped_ptr<typename Port::dataTransfer> packet;
    packet.reset(port->getPacket(bulkio::Const::NON_BLOCKING));
    CPPUNIT_ASSERT(packet);
    CPPUNIT_ASSERT(packet->sriChanged);
    CPPUNIT_ASSERT_EQUAL((size_t) 1, listener.sri.size());

    // No SRI change for second packet
    this->_pushTestPacket(1, bulkio::time::utils::now(), false, stream_id);
    CPPUNIT_ASSERT_EQUAL((size_t) 1, listener.sri.size());
    packet.reset(port->getPacket(bulkio::Const::NON_BLOCKING));
    CPPUNIT_ASSERT(packet);
    CPPUNIT_ASSERT(!packet->sriChanged);

    // Change the SRI, should flag the packet
    sri.mode = 1;
    port->pushSRI(sri);
    this->_pushTestPacket(1, bulkio::time::utils::now(), false, stream_id);
    CPPUNIT_ASSERT_EQUAL((size_t) 1, listener.sri.size());
    packet.reset(port->getPacket(bulkio::Const::NON_BLOCKING));
    CPPUNIT_ASSERT(packet);
    CPPUNIT_ASSERT(packet->sriChanged);
}

template <class Port>
void InPortTest<Port>::testSriChangedFlush()
{
    const char* stream_id = "sri_changed_flush";
    BULKIO::StreamSRI sri = bulkio::sri::create(stream_id);
    port->pushSRI(sri);

    // Reduce the queue size so we can force a flush
    port->setMaxQueueDepth(2);

    // Push a packet, change the SRI, and push two more packets so that the
    // packet with the associated SRI change gets flushed
    this->_pushTestPacket(1, bulkio::time::utils::now(), false, stream_id);
    sri.xdelta = 0.5;
    port->pushSRI(sri);
    this->_pushTestPacket(1, bulkio::time::utils::now(), false, stream_id);
    this->_pushTestPacket(1, bulkio::time::utils::now(), false, stream_id);

    // Get the last packet and verify that the queue has flushed, and the SRI
    // change is still reported
    boost::scoped_ptr<typename Port::dataTransfer> packet;
    packet.reset(port->getPacket(bulkio::Const::NON_BLOCKING));
    CPPUNIT_ASSERT(packet);
    CPPUNIT_ASSERT(packet->inputQueueFlushed);
    CPPUNIT_ASSERT(packet->sriChanged);
}

template <class Port>
void InPortTest<Port>::testSriChangedInvalidStream()
{
    const char* stream_id = "invalid_stream";

    // Turn off the port's logging to avoid dumping a warning to the screen
    port->getLogger()->setLevel(rh_logger::Level::getOff());

    SriListener listener;
    port->setNewStreamListener(&listener, &SriListener::updateSRI);
    CPPUNIT_ASSERT(listener.sri.empty());

    // Push data without an SRI to check that the sriChanged flag is still set
    // and the SRI callback gets called
    this->_pushTestPacket(100, BULKIO::PrecisionUTCTime(), false, stream_id);
    boost::scoped_ptr<typename Port::dataTransfer> packet;
    packet.reset(port->getPacket(bulkio::Const::NON_BLOCKING));
    CPPUNIT_ASSERT(packet);
    CPPUNIT_ASSERT(packet->sriChanged);
    CPPUNIT_ASSERT_EQUAL((size_t) 1, listener.sri.size());
    CPPUNIT_ASSERT_EQUAL(std::string(stream_id), std::string(listener.sri.back().streamID));

    // Push again to the same stream ID; sriChanged should now be false and the
    // SRI callback should not get called
    this->_pushTestPacket(100, BULKIO::PrecisionUTCTime(), false, stream_id);
    packet.reset(port->getPacket(bulkio::Const::NON_BLOCKING));
    CPPUNIT_ASSERT(packet);
    CPPUNIT_ASSERT(!(packet->sriChanged));
    CPPUNIT_ASSERT_EQUAL((size_t) 1, listener.sri.size());
}

template <class Port>
void InPortTest<Port>::testStatistics()
{
    // Push a packet of data to trigger meaningful statistics
    const char* stream_id = "port_stats";
    BULKIO::StreamSRI sri = bulkio::sri::create(stream_id);
    port->pushSRI(sri);
    this->_pushTestPacket(1024, bulkio::time::utils::now(), false, stream_id);

    // Check that the statistics report the right element size
    BULKIO::PortStatistics_var stats = port->statistics();
    CPPUNIT_ASSERT(stats->elementsPerSecond > 0.0);
    size_t bits_per_element = round(stats->bitsPerSecond / stats->elementsPerSecond);
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Incorrect bits per element", BITS_PER_ELEMENT, bits_per_element);
}

template <class Port>
void InPortTest<Port>::testDiscardEmptyPacket()
{
    // Push an empty, non-EOS packet
    const char* stream_id = "empty_packet";
    BULKIO::StreamSRI sri = bulkio::sri::create(stream_id);
    port->pushSRI(sri);
    this->_pushTestPacket(0, bulkio::time::utils::now(), false, stream_id);

    // No packet should be returned
    boost::scoped_ptr<PacketType> packet;
    packet.reset(port->getPacket(bulkio::Const::NON_BLOCKING));
    CPPUNIT_ASSERT(!packet);
}

template <class Port>
void NumericInPortTest<Port>::testQueueFlushScenarios()
{
    // Establish all the streams as "active"
    BULKIO::PrecisionUTCTime ts = bulkio::time::utils::create(100.0, 0.5);

    // Push 1 packet for stream_A
    BULKIO::StreamSRI stream_A = bulkio::sri::create("stream_A");
    stream_A.blocking = false;
    port->pushSRI(stream_A);
    this->_pushTestPacket(1, ts, false, stream_A.streamID);

    // Push 1 packet for stream_B
    BULKIO::StreamSRI stream_B = bulkio::sri::create("stream_B");
    stream_B.blocking = false;
    port->pushSRI(stream_B);
    this->_pushTestPacket(2, ts+0.1, false, stream_B.streamID);

    // Push 1 packet for stream_C
    BULKIO::StreamSRI stream_C = bulkio::sri::create("stream_C");
    stream_C.blocking = false;
    port->pushSRI(stream_C);
    this->_pushTestPacket(3, ts+0.2, false, stream_C.streamID);

    // empty the queue
    boost::scoped_ptr<PacketType> packet;
    packet.reset(port->getPacket(bulkio::Const::NON_BLOCKING));
    CPPUNIT_ASSERT(packet);
    CPPUNIT_ASSERT_EQUAL(std::string(stream_A.streamID), packet->streamID);
    packet.reset(port->getPacket(bulkio::Const::NON_BLOCKING));
    CPPUNIT_ASSERT(packet);
    CPPUNIT_ASSERT_EQUAL(std::string(stream_B.streamID), packet->streamID);
    packet.reset(port->getPacket(bulkio::Const::NON_BLOCKING));
    CPPUNIT_ASSERT(packet);
    CPPUNIT_ASSERT_EQUAL(std::string(stream_C.streamID), packet->streamID);

    CPPUNIT_ASSERT_EQUAL(port->getCurrentQueueDepth(), 0);
    BULKIO::StreamSRISequence_var active_sris = port->activeSRIs();
    int number_alive_streams = active_sris->length();
    CPPUNIT_ASSERT_EQUAL(number_alive_streams, 3);

    // Test case 1
    this->_pushTestPacket(4, ts+1.0, false, stream_B.streamID);
    this->_pushTestPacket(5, ts+2.0, false, stream_C.streamID);
    this->_pushTestPacket(6, ts+3.0, false, stream_B.streamID);
    this->_pushTestPacket(7, ts+4.0, false, stream_C.streamID);
    port->setMaxQueueDepth(4);
    // flush
    this->_pushTestPacket(8, ts+5.0, false, stream_A.streamID);
    packet.reset(port->getPacket(bulkio::Const::NON_BLOCKING));
    CPPUNIT_ASSERT(packet);
    CPPUNIT_ASSERT_EQUAL(std::string(stream_B.streamID), packet->streamID);
    CPPUNIT_ASSERT(packet->inputQueueFlushed);
    CPPUNIT_ASSERT(not packet->EOS);
    CPPUNIT_ASSERT(not packet->sriChanged);
    CPPUNIT_ASSERT_EQUAL(packet->T, ts+3.0);
    CPPUNIT_ASSERT_EQUAL((size_t)6, packet->dataBuffer.size());
    packet.reset(port->getPacket(bulkio::Const::NON_BLOCKING));
    CPPUNIT_ASSERT(packet);
    CPPUNIT_ASSERT_EQUAL(std::string(stream_C.streamID), packet->streamID);
    CPPUNIT_ASSERT(packet->inputQueueFlushed);
    CPPUNIT_ASSERT(not packet->EOS);
    CPPUNIT_ASSERT(not packet->sriChanged);
    CPPUNIT_ASSERT_EQUAL(packet->T, ts+4.0);
    CPPUNIT_ASSERT_EQUAL((size_t)7, packet->dataBuffer.size());
    packet.reset(port->getPacket(bulkio::Const::NON_BLOCKING));
    CPPUNIT_ASSERT(packet);
    CPPUNIT_ASSERT_EQUAL(std::string(stream_A.streamID), packet->streamID);
    CPPUNIT_ASSERT(not packet->inputQueueFlushed);
    CPPUNIT_ASSERT(not packet->EOS);
    CPPUNIT_ASSERT(not packet->sriChanged);
    CPPUNIT_ASSERT_EQUAL(packet->T, ts+5.0);
    CPPUNIT_ASSERT_EQUAL((size_t)8, packet->dataBuffer.size());

    CPPUNIT_ASSERT_EQUAL(port->getCurrentQueueDepth(), 0);
    active_sris = port->activeSRIs();
    number_alive_streams = active_sris->length();
    CPPUNIT_ASSERT_EQUAL(number_alive_streams, 3);

    // Test case 2
    port->setMaxQueueDepth(6);
    this->_pushTestPacket(7, ts+4.0, false, stream_A.streamID);
    this->_pushTestPacket(8, ts+5.0, false, stream_B.streamID);
    this->_pushTestPacket(9, ts+6.0, false, stream_C.streamID);
    this->_pushTestPacket(10, ts+7.0, false, stream_A.streamID);
    this->_pushTestPacket(11, ts+8.0, false, stream_B.streamID);
    this->_pushTestPacket(12, ts+9.0, false, stream_C.streamID);
    // flush
    this->_pushTestPacket(13, ts+10.0, false, stream_A.streamID);
    packet.reset(port->getPacket(bulkio::Const::NON_BLOCKING));
    CPPUNIT_ASSERT(packet);
    CPPUNIT_ASSERT_EQUAL(std::string(stream_B.streamID), packet->streamID);
    CPPUNIT_ASSERT(packet->inputQueueFlushed);
    CPPUNIT_ASSERT(not packet->EOS);
    CPPUNIT_ASSERT(not packet->sriChanged);
    CPPUNIT_ASSERT_EQUAL(packet->T, ts+8.0);
    CPPUNIT_ASSERT_EQUAL((size_t)11, packet->dataBuffer.size());
    packet.reset(port->getPacket(bulkio::Const::NON_BLOCKING));
    CPPUNIT_ASSERT(packet);
    CPPUNIT_ASSERT_EQUAL(std::string(stream_C.streamID), packet->streamID);
    CPPUNIT_ASSERT(packet->inputQueueFlushed);
    CPPUNIT_ASSERT(not packet->EOS);
    CPPUNIT_ASSERT(not packet->sriChanged);
    CPPUNIT_ASSERT_EQUAL(packet->T, ts+9.0);
    CPPUNIT_ASSERT_EQUAL((size_t)12, packet->dataBuffer.size());
    packet.reset(port->getPacket(bulkio::Const::NON_BLOCKING));
    CPPUNIT_ASSERT(packet);
    CPPUNIT_ASSERT_EQUAL(std::string(stream_A.streamID), packet->streamID);
    CPPUNIT_ASSERT(packet->inputQueueFlushed);
    CPPUNIT_ASSERT(not packet->EOS);
    CPPUNIT_ASSERT(not packet->sriChanged);
    CPPUNIT_ASSERT_EQUAL(packet->T, ts+10.0);
    CPPUNIT_ASSERT_EQUAL((size_t)13, packet->dataBuffer.size());

    CPPUNIT_ASSERT_EQUAL(port->getCurrentQueueDepth(), 0);
    active_sris = port->activeSRIs();
    number_alive_streams = active_sris->length();
    CPPUNIT_ASSERT_EQUAL(number_alive_streams, 3);

    port->setMaxQueueDepth(5);
    // Test case 3
    this->_pushTestPacket(11, ts+11.0, false, stream_B.streamID);
    this->_pushTestPacket(12, ts+12.0, false, stream_B.streamID);
    this->_pushTestPacket(0, ts+13.0, true, stream_B.streamID);
    this->_pushTestPacket(13, ts+14.0, false, stream_C.streamID);
    this->_pushTestPacket(14, ts+15.0, false, stream_C.streamID);
    // flush
    this->_pushTestPacket(15, ts+16.0, false, stream_A.streamID);
    packet.reset(port->getPacket(bulkio::Const::NON_BLOCKING));
    CPPUNIT_ASSERT(packet);
    CPPUNIT_ASSERT_EQUAL(std::string(stream_B.streamID), packet->streamID);
    CPPUNIT_ASSERT(packet->inputQueueFlushed);
    CPPUNIT_ASSERT(packet->EOS);
    CPPUNIT_ASSERT(not packet->sriChanged);
    CPPUNIT_ASSERT_EQUAL(packet->T, ts+12.0);
    CPPUNIT_ASSERT_EQUAL((size_t)12, packet->dataBuffer.size());
    packet.reset(port->getPacket(bulkio::Const::NON_BLOCKING));
    CPPUNIT_ASSERT(packet);
    CPPUNIT_ASSERT_EQUAL(std::string(stream_C.streamID), packet->streamID);
    CPPUNIT_ASSERT(packet->inputQueueFlushed);
    CPPUNIT_ASSERT(not packet->EOS);
    CPPUNIT_ASSERT(not packet->sriChanged);
    CPPUNIT_ASSERT_EQUAL(packet->T, ts+15.0);
    CPPUNIT_ASSERT_EQUAL((size_t)14, packet->dataBuffer.size());
    packet.reset(port->getPacket(bulkio::Const::NON_BLOCKING));
    CPPUNIT_ASSERT(packet);
    CPPUNIT_ASSERT_EQUAL(std::string(stream_A.streamID), packet->streamID);
    CPPUNIT_ASSERT(not packet->inputQueueFlushed);
    CPPUNIT_ASSERT(not packet->EOS);
    CPPUNIT_ASSERT(not packet->sriChanged);
    CPPUNIT_ASSERT_EQUAL(packet->T, ts+16.0);
    CPPUNIT_ASSERT_EQUAL((size_t)15, packet->dataBuffer.size());

    // Establish all the streams as "active"
    port->setMaxQueueDepth(3);
    // Push 1 packet for stream_A
    stream_A = bulkio::sri::create("stream_A");
    stream_A.blocking = false;
    port->pushSRI(stream_A);
    this->_pushTestPacket(14, ts+17.0, false, stream_A.streamID);

    // Push 1 packet for stream_B
    stream_B = bulkio::sri::create("stream_B");
    stream_B.blocking = false;
    port->pushSRI(stream_B);
    this->_pushTestPacket(15, ts+18.0, false, stream_B.streamID);

    // Push 1 packet for stream_C
    stream_C = bulkio::sri::create("stream_C");
    stream_C.blocking = false;
    port->pushSRI(stream_C);
    this->_pushTestPacket(16, ts+19.0, false, stream_C.streamID);

    // empty the queue
    packet.reset(port->getPacket(bulkio::Const::NON_BLOCKING));
    CPPUNIT_ASSERT(packet);
    CPPUNIT_ASSERT_EQUAL(std::string(stream_A.streamID), packet->streamID);
    packet.reset(port->getPacket(bulkio::Const::NON_BLOCKING));
    CPPUNIT_ASSERT(packet);
    CPPUNIT_ASSERT_EQUAL(std::string(stream_B.streamID), packet->streamID);
    packet.reset(port->getPacket(bulkio::Const::NON_BLOCKING));
    CPPUNIT_ASSERT(packet);
    CPPUNIT_ASSERT_EQUAL(std::string(stream_C.streamID), packet->streamID);

    CPPUNIT_ASSERT_EQUAL(port->getCurrentQueueDepth(), 0);
    active_sris = port->activeSRIs();
    number_alive_streams = active_sris->length();
    CPPUNIT_ASSERT_EQUAL(number_alive_streams, 3);

    // Test case 4
    port->setMaxQueueDepth(3);
    this->_pushTestPacket(0, ts+20.0, true, stream_B.streamID);
    this->_pushTestPacket(17, ts+21.0, false, stream_C.streamID);
    this->_pushTestPacket(18, ts+22.0, false, stream_C.streamID);
    // flush
    this->_pushTestPacket(19, ts+23.0, false, stream_A.streamID);
    packet.reset(port->getPacket(bulkio::Const::NON_BLOCKING));
    CPPUNIT_ASSERT(packet);
    CPPUNIT_ASSERT_EQUAL(std::string(stream_B.streamID), packet->streamID);
    CPPUNIT_ASSERT(not packet->inputQueueFlushed);
    CPPUNIT_ASSERT(packet->EOS);
    CPPUNIT_ASSERT(not packet->sriChanged);
    CPPUNIT_ASSERT_EQUAL(packet->T, ts+20.0);
    CPPUNIT_ASSERT(packet->dataBuffer.empty());
    packet.reset(port->getPacket(bulkio::Const::NON_BLOCKING));
    CPPUNIT_ASSERT(packet);
    CPPUNIT_ASSERT_EQUAL(std::string(stream_C.streamID), packet->streamID);
    CPPUNIT_ASSERT(packet->inputQueueFlushed);
    CPPUNIT_ASSERT(not packet->EOS);
    CPPUNIT_ASSERT(not packet->sriChanged);
    CPPUNIT_ASSERT_EQUAL(packet->T, ts+22.0);
    CPPUNIT_ASSERT_EQUAL((size_t)18, packet->dataBuffer.size());
    packet.reset(port->getPacket(bulkio::Const::NON_BLOCKING));
    CPPUNIT_ASSERT(packet);
    CPPUNIT_ASSERT_EQUAL(std::string(stream_A.streamID), packet->streamID);
    CPPUNIT_ASSERT(not packet->inputQueueFlushed);
    CPPUNIT_ASSERT(not packet->EOS);
    CPPUNIT_ASSERT(not packet->sriChanged);
    CPPUNIT_ASSERT_EQUAL(packet->T, ts+23.0);
    CPPUNIT_ASSERT_EQUAL((size_t)19, packet->dataBuffer.size());

    // Establish all the streams as "active"
    port->setMaxQueueDepth(3);
    // Push 1 packet for stream_A
    stream_A = bulkio::sri::create("stream_A");
    stream_A.blocking = false;
    port->pushSRI(stream_A);
    this->_pushTestPacket(19, ts+24.0, false, stream_A.streamID);

    // Push 1 packet for stream_B
    stream_B = bulkio::sri::create("stream_B");
    stream_B.blocking = false;
    port->pushSRI(stream_B);
    this->_pushTestPacket(20, ts+25.0, false, stream_B.streamID);

    // Push 1 packet for stream_C
    stream_C = bulkio::sri::create("stream_C");
    stream_C.blocking = false;
    port->pushSRI(stream_C);
    this->_pushTestPacket(21, ts+26.0, false, stream_C.streamID);

    // empty the queue
    packet.reset(port->getPacket(bulkio::Const::NON_BLOCKING));
    CPPUNIT_ASSERT(packet);
    CPPUNIT_ASSERT_EQUAL(std::string(stream_A.streamID), packet->streamID);
    packet.reset(port->getPacket(bulkio::Const::NON_BLOCKING));
    CPPUNIT_ASSERT(packet);
    CPPUNIT_ASSERT_EQUAL(std::string(stream_B.streamID), packet->streamID);
    packet.reset(port->getPacket(bulkio::Const::NON_BLOCKING));
    CPPUNIT_ASSERT(packet);
    CPPUNIT_ASSERT_EQUAL(std::string(stream_C.streamID), packet->streamID);

    CPPUNIT_ASSERT_EQUAL(port->getCurrentQueueDepth(), 0);
    active_sris = port->activeSRIs();
    number_alive_streams = active_sris->length();
    CPPUNIT_ASSERT_EQUAL(number_alive_streams, 3);

    // Test case 5
    port->setMaxQueueDepth(3);
    this->_pushTestPacket(22, ts+27.0, false, stream_A.streamID);
    this->_pushTestPacket(0, ts+28.0, true, stream_A.streamID);
    port->pushSRI(stream_A);
    this->_pushTestPacket(23, ts+29.0, false, stream_A.streamID);
    // flush
    this->_pushTestPacket(24, ts+30.0, false, stream_A.streamID);
    packet.reset(port->getPacket(bulkio::Const::NON_BLOCKING));
    CPPUNIT_ASSERT(packet);
    CPPUNIT_ASSERT_EQUAL(std::string(stream_A.streamID), packet->streamID);
    CPPUNIT_ASSERT(not packet->inputQueueFlushed);
    CPPUNIT_ASSERT(packet->EOS);
    CPPUNIT_ASSERT(not packet->sriChanged);
    CPPUNIT_ASSERT_EQUAL(packet->T, ts+27.0);
    CPPUNIT_ASSERT_EQUAL((size_t)22, packet->dataBuffer.size());
    packet.reset(port->getPacket(bulkio::Const::NON_BLOCKING));
    CPPUNIT_ASSERT(packet);
    CPPUNIT_ASSERT_EQUAL(std::string(stream_A.streamID), packet->streamID);
    CPPUNIT_ASSERT(packet->inputQueueFlushed);
    CPPUNIT_ASSERT(not packet->EOS);
    CPPUNIT_ASSERT(packet->sriChanged);
    CPPUNIT_ASSERT_EQUAL(packet->T, ts+30.0);
    CPPUNIT_ASSERT_EQUAL((size_t)24, packet->dataBuffer.size());

    CPPUNIT_ASSERT_EQUAL(port->getCurrentQueueDepth(), 0);
    active_sris = port->activeSRIs();
    number_alive_streams = active_sris->length();
    CPPUNIT_ASSERT_EQUAL(number_alive_streams, 3);

    // Test case 5a
    port->setMaxQueueDepth(4);
    this->_pushTestPacket(21, ts+31.0, false, stream_A.streamID);
    this->_pushTestPacket(22, ts+32.0, false, stream_A.streamID);
    this->_pushTestPacket(0, ts+33.0, true, stream_A.streamID);
    port->pushSRI(stream_A);
    this->_pushTestPacket(23, ts+34.0, false, stream_A.streamID);
    // flush
    this->_pushTestPacket(24, ts+35.0, false, stream_A.streamID);
    packet.reset(port->getPacket(bulkio::Const::NON_BLOCKING));
    CPPUNIT_ASSERT(packet);
    CPPUNIT_ASSERT_EQUAL(std::string(stream_A.streamID), packet->streamID);
    CPPUNIT_ASSERT(packet->inputQueueFlushed);
    CPPUNIT_ASSERT(packet->EOS);
    CPPUNIT_ASSERT(not packet->sriChanged);
    CPPUNIT_ASSERT_EQUAL(packet->T, ts+32.0);
    CPPUNIT_ASSERT_EQUAL((size_t)22, packet->dataBuffer.size());
    packet.reset(port->getPacket(bulkio::Const::NON_BLOCKING));
    CPPUNIT_ASSERT(packet);
    CPPUNIT_ASSERT_EQUAL(std::string(stream_A.streamID), packet->streamID);
    CPPUNIT_ASSERT(packet->inputQueueFlushed);
    CPPUNIT_ASSERT(not packet->EOS);
    CPPUNIT_ASSERT(packet->sriChanged);
    CPPUNIT_ASSERT_EQUAL(packet->T, ts+35.0);
    CPPUNIT_ASSERT_EQUAL((size_t)24, packet->dataBuffer.size());

    CPPUNIT_ASSERT_EQUAL(port->getCurrentQueueDepth(), 0);
    active_sris = port->activeSRIs();
    number_alive_streams = active_sris->length();
    CPPUNIT_ASSERT_EQUAL(number_alive_streams, 3);

    // Test case 6
    port->setMaxQueueDepth(3);
    this->_pushTestPacket(0, ts+36.0, true, stream_A.streamID);
    this->_pushTestPacket(25, ts+37.0, false, stream_B.streamID);
    this->_pushTestPacket(26, ts+38.0, false, stream_B.streamID);
    // flush
    port->pushSRI(stream_A);
    this->_pushTestPacket(27, ts+39.0, false, stream_A.streamID);
    packet.reset(port->getPacket(bulkio::Const::NON_BLOCKING));
    CPPUNIT_ASSERT(packet);
    CPPUNIT_ASSERT_EQUAL(std::string(stream_A.streamID), packet->streamID);
    CPPUNIT_ASSERT(not packet->inputQueueFlushed);
    CPPUNIT_ASSERT(packet->EOS);
    CPPUNIT_ASSERT(not packet->sriChanged);
    CPPUNIT_ASSERT(packet->dataBuffer.empty());
    CPPUNIT_ASSERT_EQUAL(packet->T, ts+36.0);
    packet.reset(port->getPacket(bulkio::Const::NON_BLOCKING));
    CPPUNIT_ASSERT(packet);
    CPPUNIT_ASSERT_EQUAL(std::string(stream_B.streamID), packet->streamID);
    CPPUNIT_ASSERT(packet->inputQueueFlushed);
    CPPUNIT_ASSERT(not packet->EOS);
    CPPUNIT_ASSERT(not packet->sriChanged);
    CPPUNIT_ASSERT_EQUAL(packet->T, ts+38.0);
    CPPUNIT_ASSERT_EQUAL((size_t)26, packet->dataBuffer.size());
    packet.reset(port->getPacket(bulkio::Const::NON_BLOCKING));
    CPPUNIT_ASSERT(packet);
    CPPUNIT_ASSERT_EQUAL(std::string(stream_A.streamID), packet->streamID);
    CPPUNIT_ASSERT(not packet->inputQueueFlushed);
    CPPUNIT_ASSERT(not packet->EOS);
    CPPUNIT_ASSERT(packet->sriChanged);
    CPPUNIT_ASSERT_EQUAL(packet->T, ts+39.0);
    CPPUNIT_ASSERT_EQUAL((size_t)27, packet->dataBuffer.size());

    CPPUNIT_ASSERT_EQUAL(port->getCurrentQueueDepth(), 0);
    active_sris = port->activeSRIs();
    number_alive_streams = active_sris->length();
    CPPUNIT_ASSERT_EQUAL(number_alive_streams, 3);

    port->setMaxQueueDepth(2);
    if (stream_A.mode) {
        stream_A.mode = 0;
    } else {
        stream_A.mode = 1;
    }
    port->pushSRI(stream_A);
    this->_pushTestPacket(28, ts+40.0, false, stream_A.streamID);
    packet.reset(port->getPacket(bulkio::Const::NON_BLOCKING));
    CPPUNIT_ASSERT(packet);
    CPPUNIT_ASSERT_EQUAL(std::string(stream_A.streamID), packet->streamID);
    CPPUNIT_ASSERT(not packet->inputQueueFlushed);
    CPPUNIT_ASSERT(not packet->EOS);
    CPPUNIT_ASSERT(packet->sriChanged);
    CPPUNIT_ASSERT_EQUAL(packet->T, ts+40.0);
    CPPUNIT_ASSERT_EQUAL((size_t)28, packet->dataBuffer.size());

    // Test case 7
    port->setMaxQueueDepth(4);
    if (stream_A.mode) {
        stream_A.mode = 0;
    } else {
        stream_A.mode = 1;
    }
    port->pushSRI(stream_A);
    this->_pushTestPacket(28, ts+41.0, false, stream_A.streamID);
    this->_pushTestPacket(29, ts+42.0, false, stream_C.streamID);
    this->_pushTestPacket(30, ts+43.0, false, stream_A.streamID);
    this->_pushTestPacket(31, ts+44.0, false, stream_C.streamID);
    // flush
    this->_pushTestPacket(32, ts+45.0, false, stream_B.streamID);
    packet.reset(port->getPacket(bulkio::Const::NON_BLOCKING));
    CPPUNIT_ASSERT(packet);
    CPPUNIT_ASSERT_EQUAL(std::string(stream_A.streamID), packet->streamID);
    CPPUNIT_ASSERT(packet->inputQueueFlushed);
    CPPUNIT_ASSERT(not packet->EOS);
    CPPUNIT_ASSERT(packet->sriChanged);
    CPPUNIT_ASSERT_EQUAL(packet->T, ts+43.0);
    CPPUNIT_ASSERT_EQUAL((size_t)30, packet->dataBuffer.size());
    packet.reset(port->getPacket(bulkio::Const::NON_BLOCKING));
    CPPUNIT_ASSERT(packet);
    CPPUNIT_ASSERT_EQUAL(std::string(stream_C.streamID), packet->streamID);
    CPPUNIT_ASSERT(packet->inputQueueFlushed);
    CPPUNIT_ASSERT(not packet->EOS);
    CPPUNIT_ASSERT(not packet->sriChanged);
    CPPUNIT_ASSERT_EQUAL(packet->T, ts+44.0);
    CPPUNIT_ASSERT_EQUAL((size_t)31, packet->dataBuffer.size());
    packet.reset(port->getPacket(bulkio::Const::NON_BLOCKING));
    CPPUNIT_ASSERT(packet);
    CPPUNIT_ASSERT_EQUAL(std::string(stream_B.streamID), packet->streamID);
    CPPUNIT_ASSERT(not packet->inputQueueFlushed);
    CPPUNIT_ASSERT(not packet->EOS);
    CPPUNIT_ASSERT(not packet->sriChanged);
    CPPUNIT_ASSERT_EQUAL(packet->T, ts+45.0);
    CPPUNIT_ASSERT_EQUAL((size_t)32, packet->dataBuffer.size());

    CPPUNIT_ASSERT_EQUAL(port->getCurrentQueueDepth(), 0);
    active_sris = port->activeSRIs();
    number_alive_streams = active_sris->length();
    CPPUNIT_ASSERT_EQUAL(number_alive_streams, 3);

    // Test case 8
    port->setMaxQueueDepth(3);
    if (stream_A.mode) {
        stream_A.mode = 0;
    } else {
        stream_A.mode = 1;
    }
    port->pushSRI(stream_A);
    this->_pushTestPacket(28, ts+46.0, false, stream_A.streamID);
    this->_pushTestPacket(29, ts+47.0, false, stream_C.streamID);
    this->_pushTestPacket(30, ts+48.0, false, stream_A.streamID);
    // flush
    this->_pushTestPacket(32, ts+49.0, false, stream_B.streamID);
    packet.reset(port->getPacket(bulkio::Const::NON_BLOCKING));
    CPPUNIT_ASSERT(packet);
    CPPUNIT_ASSERT_EQUAL(std::string(stream_C.streamID), packet->streamID);
    CPPUNIT_ASSERT(not packet->inputQueueFlushed);
    CPPUNIT_ASSERT(not packet->EOS);
    CPPUNIT_ASSERT(not packet->sriChanged);
    CPPUNIT_ASSERT_EQUAL(packet->T, ts+47.0);
    CPPUNIT_ASSERT_EQUAL((size_t)29, packet->dataBuffer.size());
    packet.reset(port->getPacket(bulkio::Const::NON_BLOCKING));
    CPPUNIT_ASSERT(packet);
    CPPUNIT_ASSERT_EQUAL(std::string(stream_A.streamID), packet->streamID);
    CPPUNIT_ASSERT(packet->inputQueueFlushed);
    CPPUNIT_ASSERT(not packet->EOS);
    CPPUNIT_ASSERT(packet->sriChanged);
    CPPUNIT_ASSERT_EQUAL(packet->T, ts+48.0);
    CPPUNIT_ASSERT_EQUAL((size_t)30, packet->dataBuffer.size());
    packet.reset(port->getPacket(bulkio::Const::NON_BLOCKING));
    CPPUNIT_ASSERT(packet);
    CPPUNIT_ASSERT_EQUAL(std::string(stream_B.streamID), packet->streamID);
    CPPUNIT_ASSERT(not packet->inputQueueFlushed);
    CPPUNIT_ASSERT(not packet->EOS);
    CPPUNIT_ASSERT(not packet->sriChanged);
    CPPUNIT_ASSERT_EQUAL(packet->T, ts+49.0);
    CPPUNIT_ASSERT_EQUAL((size_t)32, packet->dataBuffer.size());

    CPPUNIT_ASSERT_EQUAL(port->getCurrentQueueDepth(), 0);
    active_sris = port->activeSRIs();
    number_alive_streams = active_sris->length();
    CPPUNIT_ASSERT_EQUAL(number_alive_streams, 3);
}

template <class Port>
void InPortTest<Port>::testQueueFlushFlags()
{
    // Push 1 packet for the normal data stream
    BULKIO::StreamSRI sri_data = bulkio::sri::create("stream_data");
    sri_data.blocking = false;
    port->pushSRI(sri_data);
    this->_pushTestPacket(1, bulkio::time::utils::now(), false, sri_data.streamID);

    // Push 1 packet for the EOS test stream
    BULKIO::StreamSRI sri_eos = bulkio::sri::create("stream_eos");
    sri_eos.blocking = false;
    port->pushSRI(sri_eos);
    this->_pushTestPacket(1, bulkio::time::utils::now(), false, sri_eos.streamID);

    // Push 1 packet for the SRI change stream
    BULKIO::StreamSRI sri_change = bulkio::sri::create("stream_sri");
    sri_change.blocking = false;
    sri_change.mode = 0;
    port->pushSRI(sri_change);
    this->_pushTestPacket(1, bulkio::time::utils::now(), false, sri_change.streamID);

    // Grab the packets to ensure the initial conditions are correct
    boost::scoped_ptr<PacketType> packet;
    packet.reset(port->getPacket(bulkio::Const::NON_BLOCKING));
    CPPUNIT_ASSERT(packet);
    CPPUNIT_ASSERT_EQUAL(std::string(sri_data.streamID), packet->streamID);

    packet.reset(port->getPacket(bulkio::Const::NON_BLOCKING));
    CPPUNIT_ASSERT(packet);
    CPPUNIT_ASSERT_EQUAL(std::string(sri_eos.streamID), packet->streamID);

    packet.reset(port->getPacket(bulkio::Const::NON_BLOCKING));
    CPPUNIT_ASSERT(packet);
    CPPUNIT_ASSERT_EQUAL(std::string(sri_change.streamID), packet->streamID);

    // Push an EOS packet for the EOS stream
    this->_pushTestPacket(0, bulkio::time::utils::notSet(), true, sri_eos.streamID);

    // Modify the SRI for the SRI change stream and push another packet
    sri_change.mode = 1;
    port->pushSRI(sri_change);
    this->_pushTestPacket(2, bulkio::time::utils::now(), false, sri_change.streamID);

    this->_pushTestPacket(3, bulkio::time::utils::now(), false, sri_change.streamID);

    // Cause a queue flush by lowering the ceiling and pushing packets
    port->setMaxQueueDepth(4);

    this->_pushTestPacket(4, bulkio::time::utils::now(), false, sri_data.streamID);
    // the following push forces the flush
    this->_pushTestPacket(5, bulkio::time::utils::now(), false, sri_data.streamID);
    port->setMaxQueueDepth(6);

    // Push another packet for the SRI change stream
    this->_pushTestPacket(6, bulkio::time::utils::now(), false, sri_change.streamID);

    // 1st packet should be for EOS stream, with no data or SRI change flag
    packet.reset(port->getPacket(bulkio::Const::NON_BLOCKING));
    CPPUNIT_ASSERT(packet);
    CPPUNIT_ASSERT_EQUAL(std::string(sri_eos.streamID), packet->streamID);
    CPPUNIT_ASSERT(not packet->inputQueueFlushed);
    CPPUNIT_ASSERT(packet->EOS);
    CPPUNIT_ASSERT(not packet->sriChanged);
    CPPUNIT_ASSERT(packet->dataBuffer.empty());

    // 2nd packet should be for no EOS stream, data from the third packet on the queue, with "lost" SRI change flag from the second packet
    packet.reset(port->getPacket(bulkio::Const::NON_BLOCKING));
    CPPUNIT_ASSERT(packet);
    CPPUNIT_ASSERT_EQUAL(std::string(sri_change.streamID), packet->streamID);
    CPPUNIT_ASSERT(packet->inputQueueFlushed);
    CPPUNIT_ASSERT(not packet->EOS);
    CPPUNIT_ASSERT(packet->sriChanged);
    CPPUNIT_ASSERT_EQUAL((size_t)3, packet->dataBuffer.size());

    // 3rd packet should be for no EOS stream, with data (since it did the push that flushed the queue), no SRI change flag
    packet.reset(port->getPacket(bulkio::Const::NON_BLOCKING));
    CPPUNIT_ASSERT(packet);
    CPPUNIT_ASSERT_EQUAL(std::string(sri_data.streamID), packet->streamID);
    CPPUNIT_ASSERT(packet->inputQueueFlushed);
    CPPUNIT_ASSERT(!packet->EOS);
    CPPUNIT_ASSERT(!packet->sriChanged);
    CPPUNIT_ASSERT_EQUAL((size_t)5, packet->dataBuffer.size());

    // 4th packet should be for stream_sri stream, with no EOS or SRI change flag
    packet.reset(port->getPacket(bulkio::Const::NON_BLOCKING));
    CPPUNIT_ASSERT(packet);
    CPPUNIT_ASSERT_EQUAL(std::string(sri_change.streamID), packet->streamID);
    CPPUNIT_ASSERT(not packet->inputQueueFlushed);
    CPPUNIT_ASSERT(not packet->EOS);
    CPPUNIT_ASSERT(not packet->sriChanged);
    CPPUNIT_ASSERT_EQUAL((size_t)6, packet->dataBuffer.size());
}

template <class Port>
void InPortTest<Port>::testQueueSize()
{
    BULKIO::StreamSRI sri = bulkio::sri::create("queue_size");
    port->pushSRI(sri);

    // Start with a reasonably small queue depth and check that a flush occurs at
    // the expected time
    port->setMaxQueueDepth(10);
    for (int ii = 0; ii < 10; ++ii) {
        this->_pushTestPacket(1, bulkio::time::utils::now(), false, sri.streamID);
    }
    CPPUNIT_ASSERT_EQUAL(10, port->getCurrentQueueDepth());
    // force a flush
    this->_pushTestPacket(2, bulkio::time::utils::now(), false, sri.streamID);
    // the queue has the last "flushed" packet and the packet that caused the flush
    CPPUNIT_ASSERT_EQUAL(1, port->getCurrentQueueDepth());

    // pop the last "flushed" packet
    boost::scoped_ptr<PacketType> packet;
    packet.reset(port->getPacket(bulkio::Const::NON_BLOCKING));
    CPPUNIT_ASSERT(packet);
    CPPUNIT_ASSERT(packet->inputQueueFlushed);
    CPPUNIT_ASSERT_EQUAL((size_t)2, packet->dataBuffer.size());

    CPPUNIT_ASSERT_EQUAL(0, port->getCurrentQueueDepth());

    // Set queue depth to unlimited and push a lot of packets
    port->setMaxQueueDepth(-1);
    const int QUEUE_SIZE = 250;
    for (int ii = 0; ii < QUEUE_SIZE; ++ii) {
        this->_pushTestPacket(1, bulkio::time::utils::now(), false, sri.streamID);
    }
    CPPUNIT_ASSERT_EQUAL(QUEUE_SIZE, port->getCurrentQueueDepth());
    for (int ii = 0; ii < QUEUE_SIZE; ++ii) {
        packet.reset(port->getPacket(bulkio::Const::NON_BLOCKING));
        CPPUNIT_ASSERT(packet);
        CPPUNIT_ASSERT(!packet->inputQueueFlushed);
    }
}

/*#define CREATE_TEST(x,BITS)                                             \
    class In##x##PortTest : public InPortTest<bulkio::In##x##Port>      \
    {                                                                   \
        CPPUNIT_TEST_SUB_SUITE(In##x##PortTest, InPortTest<bulkio::In##x##Port>); \
        CPPUNIT_TEST_SUITE_END();                                       \
    };                                                                  \
    template <>                                                         \
    const size_t InPortTest<bulkio::In##x##Port>::BITS_PER_ELEMENT = BITS; \
    CPPUNIT_TEST_SUITE_REGISTRATION(In##x##PortTest);
*/

#define CREATE_TEST(x, BASE, BITS)                                            \
    class In##x##PortTest : public BASE<bulkio::In##x##Port>          \
    {                                                                   \
        CPPUNIT_TEST_SUB_SUITE(In##x##PortTest, BASE<bulkio::In##x##Port>); \
        CPPUNIT_TEST_SUITE_END();                                       \
    };                                                                  \
    template <>                                                         \
    const size_t InPortTest<bulkio::In##x##Port>::BITS_PER_ELEMENT = BITS; \
    CPPUNIT_TEST_SUITE_REGISTRATION(In##x##PortTest);

#define CREATE_BASIC_TEST(x, BITS) CREATE_TEST(x, InPortTest, BITS)
#define CREATE_NUMERIC_TEST(x, BITS) CREATE_TEST(x, NumericInPortTest, BITS)

CREATE_NUMERIC_TEST(Octet, 8);
CREATE_NUMERIC_TEST(Char, 8);
CREATE_NUMERIC_TEST(Short, 16);
CREATE_NUMERIC_TEST(UShort, 16);
CREATE_NUMERIC_TEST(Long, 32);
CREATE_NUMERIC_TEST(ULong, 32);
CREATE_NUMERIC_TEST(LongLong, 64);
CREATE_NUMERIC_TEST(ULongLong, 64);
CREATE_NUMERIC_TEST(Float, 32);
CREATE_NUMERIC_TEST(Double, 64);
CREATE_NUMERIC_TEST(Bit, 1);
CREATE_BASIC_TEST(XML, 8);
CREATE_BASIC_TEST(File, 8);
