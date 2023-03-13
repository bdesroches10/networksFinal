# Networks Final Project
Advanced Computer Networks final project files.

For this project, we have been challenged to create a peer-to-peer application using UDP and TCP sockets.
The application consists of a single index server, and a generic peer program that can be deployed on multiple machines to make use of this index server.
This application allows for peers to exchange content among one another through the coordination of the index server.
For the purpose of this project, we chose to focus on the transfer of text files as the main feat of this project is to create the application in the style of peer-to-peer, and this structure would be true regardless of the type of content being sent/received.
When a peer has a piece of content available, it has the ability to register that content with the index server such that other peers can connect to and download the peers content.
This connection for downloading content from other peers is based on TCP while the main communication between peer and index server is based on UDP.
