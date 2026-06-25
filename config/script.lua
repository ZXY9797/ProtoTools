-- ProtoDebug Lua 脚本
-- 收到帧数据时自动调用 on_data_recv(frame)
-- 发送帧数据时自动调用 on_data_send(frame)

-- frame 包含的字段:
--   timestamp, direction, sender, receiver, seq
--   type (REQ/ACK), cmdSet, cmdId, data, crc

function on_data_recv(frame)
  if frame.type == "ACK" then
    log("收到 ACK: " .. frame.cmdSet .. frame.cmdId)
    log("  Seq: " .. frame.seq .. "  Data: " .. frame.data)
  end
end

function on_data_send(frame)
  log("发送: " .. frame.cmdSet .. frame.cmdId .. " Seq:" .. frame.seq)
end
