#!/usr/bin/env python

# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# The scheme an external containerizer has to adhere to is;
#
# COMMAND (ADDITIONAL-PARAMETERS) < INPUT-PROTO > RESULT-PROTO
#
# launch (ContainerID) < ExternalTask > ExternalStatus
# update (ContainerID) < ResourceArray > ExternalStatus
# usage (ContainerID) > ResourceStatistics
# wait (ContainerID) > Termination
# destroy (ContainerID) > ExternalStatus
#
# NOTE: ExternalStatus is currently used for synchronizing and human
# readable logging. The embedded message does not have to adhere to
# any scheme but must not be empty for valid results.
#
# NOTE: After delivering a protobuf via stdout, that stream has to be
# closed to signal a complete transmission.
#
# A complete implementations is only required for 'launch'. All other
# command implementations may simply return a zero exit code and
# thereby trigger a fall back for that specific command within Mesos.
# See 'destroy' and 'wait' for examples of fall back implementation
# triggers.
# NOTE: 'update' does not have a fall back implementation and is
# simply ignored if not implemented within the external containerizer
# script.

import os
import subprocess
import sys
import struct

import multiprocessing
import time

# Render a string describing how to use this script.
def use(container, methods):
    out = "Usage: %s <command> <container-id>\n" % container
    out += "Valid commands: " + ', '.join(methods.keys())

    return out


# Read a data chunk prepended by its total size from stdin.
def receive():
    # Read size (size_t = unsigned long long => 8 bytes).
    size = struct.unpack('Q', sys.stdin.read(8))
    if size[0] <= 0:
        print >> sys.stderr, "Expected protobuf size over stdin. " \
                             "Received 0 bytes."
        return ""

    # Read payload.
    data = sys.stdin.read(size[0])
    if len(data) != size[0]:
        print >> sys.stderr, "Expected %d bytes protobuf over stdin. " \
                             "Received %d bytes." % (size[0], len(data))
        return ""

    return data


# Write a protobuf message prepended by its total size to stdout.
def send(data):
    # Write size (unsigned long long = size_t).
    os.write(1, struct.pack('Q', len(data)))

    # Write payload.
    os.write(1, data)


# Start a containerized executor.
# Expects to receive an ExternalTask protobuf via stdin and will deliver
# an ExternalStatus protobuf via stdout when successful.
def launch(container, arguments):
    try:
        data = receive()
        if len(data) == 0:
            return 1

        external = mesos_pb2.ExternalTask()
        external.ParseFromString(data)

        if external.task.HasField("executor"):
            command = ["sh",
                       "-c",
                       external.task.executor.command.value]
        else:
            print >> sys.stderr, "No executor passed; using mesos_executor!"
            command = [external.mesos_executor_path,
                       "sh",
                       "-c",
                       external.task.command.value]

        proc = subprocess.Popen(command, env=os.environ.copy())

        status = mesos_pb2.ExternalStatus();
        status.message = "test-containerizer reports on launch."
        status.pid = proc.pid

        send(status.SerializeToString());

    except google.protobuf.message.EncodeError:
        print >> sys.stderr, "Could not serialise ExternalStatus protobuf."
        return 1

    except google.protobuf.message.DecodeError:
        print >> sys.stderr, "Could not deserialise ExternalTask protobuf"
        return 1

    except OSError as e:
        print >> sys.stderr, e.strerror
        return 1

    except ValueError:
        print >> sys.stderr, "Value is invalid"
        return 1

    return 0


# Update the container's resources.
# Expects to receive a ResourceArray protobuf via stdin and will
# deliver an ExternalStatus protobuf via stdout when successful.
def update(container, arguments):
    try:
        data = receive()
        if len(data) == 0:
            return 1

        resources = mesos_pb2.ResourceArray()
        resources.ParseFromString(data)

        print >> sys.stderr, "Received " + str(len(resources.resource)) \
                           + " resource elements."

        status = mesos_pb2.ExternalStatus();
        status.message = "test-containerizer reports on update.";

        send(status.SerializeToString());

    except google.protobuf.message.EncodeError:
        print >> sys.stderr, "Could not serialise ExternalStatus protobuf."
        return 1

    except google.protobuf.message.DecodeError:
        print >> sys.stderr, "Could not deserialise ResourceArray protobuf."
        return 1

    except OSError as e:
        print >> sys.stderr, e.strerror
        return 1

    except ValueError:
        print >> sys.stderr, "Value is invalid"
        return 1

    return 0


# Gather resource usage statistics for the containerized executor.
# Delivers an ResourceStatistics protobut via stdout when
# successful.
def usage(container, arguments):
    try:
        statistics = mesos_pb2.ResourceStatistics();

        statistics.timestamp = time.time();

        # Cook up some fake data.
        statistics.mem_rss_bytes = 1073741824;
        statistics.mem_limit_bytes = 1073741824;
        statistics.cpus_limit = 2;
        statistics.cpus_user_time_secs = 0.12;
        statistics.cpus_system_time_secs = 0.5;

        send(statistics.SerializeToString());

    except google.protobuf.message.EncodeError:
        print >> sys.stderr, "Could not serialise ResourceStatistics protobuf."
        return 1

    except OSError as e:
        print >> sys.stderr, e.strerror
        return 1

    return 0


# Terminate the containerized executor.
# A complete implementation would deliver an ExternalStatus protobuf
# when succesful.
def destroy(container, arguments):
    return 0


# Get the containerized executor's Termination.
# A complete implementation would deliver a Termination protobuf
# filled with the information gathered from launch's waitpid via
# stdout.
def wait(container, arguments):
    return 0


if __name__ == "__main__":
    methods = { "launch":  launch,
                "update":  update,
                "destroy": destroy,
                "usage":   usage,
                "wait":    wait }

    if sys.argv[1:2] == ["--help"] or sys.argv[1:2] == ["-h"]:
        print use(sys.argv[0], methods)
        sys.exit(0)

    if len(sys.argv) < 3:
        print >> sys.stderr, "Please pass a command and a container-id"
        print >> sys.stderr, use(sys.argv[0], methods)
        sys.exit(1)

    command = sys.argv[1]
    if command not in methods:
        print >> sys.stderr, "No valid command passed"
        print >> sys.stderr, use(sys.argv[0], methods)
        sys.exit(2)

    method = methods.get(command)

    import mesos
    import mesos_pb2
    import google

    sys.exit(method(sys.argv[2], sys.argv[3:]))
