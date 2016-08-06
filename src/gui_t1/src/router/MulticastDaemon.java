/**
 * Redes Integradas de Telecomunicações I
 * MIEEC 2015/2016
 *
 * MulticastDaemon.java
 *
 * Class that supports multicast communication
 *
 * @author  Luis Bernardo
 */
package router;

import java.io.*;
import java.net.*;

/**
 *
 * @author user
 */
public class MulticastDaemon extends Thread {
        volatile boolean keepRunning = true;
        private DatagramSocket ds;
        private MulticastSocket ms;
        private String multicast_addr;
        private InetAddress group;
        private int mport;
        private router win;
        private routing route;

        /** Constructor */
        MulticastDaemon(DatagramSocket ds, String multicast_addr, int mport,
                router win, routing route) {
            this.ds = ds;
            this.multicast_addr = multicast_addr;
            this.mport = mport;
            this.win = win;
            this.route = route;
            try {
                // Starts the multicast socket
                ms = new MulticastSocket(mport);
                group = InetAddress.getByName(multicast_addr);
                ms.joinGroup(group);
            } catch (Exception e) {
                win.Log("Multicast daemon failure: " + e + "\n");
                if (ms != null) {
                    ms.close();
                    ms = null;
                }
                mport = -1;
                group = null;
            }

        }

        // Test object
        public boolean valid() {
            return ms != null;
        }

        /** Sends packet to group */
        public void send_packet(DatagramPacket dp) throws IOException {
            if (!valid()) {
                win.Log("Invalid call to send_packet multicast\n");
                return;
            }
            try {
                dp.setAddress(group);
                dp.setPort(mport);
                ds.send(dp);
            } catch (IOException e) {
                throw e;
            }
            // win.Log("mpacket sent to " + mport + "\n");
        }

        // Thread main function
        @Override
        public void run() {
            byte[] buf = new byte[8096];
            DatagramPacket dp = new DatagramPacket(buf, buf.length);
            try {
                while (keepRunning) {
                    try {
                        ms.receive(dp);
                        ByteArrayInputStream BAis =
                                new ByteArrayInputStream(buf, 0, dp.getLength());
                        DataInputStream dis = new DataInputStream(BAis);
                        System.out.println("Received mpacket (" + dp.getLength()
                                + ") from " + dp.getAddress().getHostAddress()
                                + ":" + dp.getPort());
                        byte code;
                        char sender;
                        int area;
                        try {
                            code = dis.readByte();     // read code
                            sender = dis.readChar();   // read sender id
                            String ip = dp.getAddress().getHostAddress();  // Get sender address
                            switch (code) {
                                case router.PKT_ROUTE:
                                    route.process_multicast_ROUTE(sender,
                                            dp, ip, dis);
                                    break;
                                default:
                                    win.Log("Invalid mpacket type: " + code + "\n");
                            }
                        } catch (IOException e) {
                            win.Log("Multicast Packet too short\n");
                        }
                    } catch (SocketException se) {
                        if (keepRunning) {
                            win.Log("recv UDP SocketException : " + se + "\n");
                        }
                    }
                }
            } catch (IOException e) {
                if (keepRunning) {
                    win.Log("IO exception receiving data from socket : " + e);
                }
            }
        }

        // Stops thread
        public void stopRunning() {
            keepRunning = false;
            try {
                InetAddress _group = InetAddress.getByName(multicast_addr);
                ms.leaveGroup(_group);
            } catch (UnknownHostException e) {
                win.Log("Invalid address in stop running '" + multicast_addr + "': " + e + "\n");
            } catch (IOException e) {
                win.Log("Failed leave group: " + e + "\n");
            }
            if (this.isAlive()) {
                this.interrupt();
            }
        }

}
