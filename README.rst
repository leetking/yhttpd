======
yhttpd
======

A simple but powerful http server, for studying ``HTTP`` protocol, epoll and linux.

Usage
-----

.. code-block:: bash

  $ ./yhttp -h

The default structure of ``www`` directory is shown as below.

.. code-block:: text

  www/
  |-- err-codes/
  |   +-- 404.html    # etc.
  |-- cgi-bin/
  |   +-- dopost.lua
  |
  +-- index.html      # normal file

Thanks
------

1. Inspire by ``Tiny-httpd`` *J. David Blackstone*.
#. ``lighttpd``
#. ``boa server``

Special Thanks
--------------

1. ``Nginx`` The most desgin is from it

