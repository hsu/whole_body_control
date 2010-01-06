/*
 * Copyright (c) 2010 Stanford University
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program.  If not, see
 * <http://www.gnu.org/licenses/>
 */

/**
   \file XMLRPCDirectoryServer.hpp
   \author Roland Philippsen
*/

#ifndef WBC_XMLRPC_DIRECTORY_HPP
#define WBC_XMLRPC_DIRECTORY_HPP

#include <wbc/bin/directory.hpp>

namespace XmlRpc {
  class XmlRpcServer;
  class XmlRpcServerMethod;
}

namespace wbc {  
  
  class XMLRPCDirectoryServer
  {
  public:
    explicit XMLRPCDirectoryServer(Directory * dir);
    ~XMLRPCDirectoryServer();
    
    Directory * GetDirectory();
    XmlRpc::XmlRpcServer * GetServer();
    void RunForever(int port);
    
  protected:
    Directory * m_dir;
    XmlRpc::XmlRpcServer * m_server;
    std::vector<XmlRpc::XmlRpcServerMethod *> m_methods;
  };
  
}

#endif // WBC_XMLRPC_DIRECTORY_HPP
