#!/usr/bin/env ruby

require "pp"

require "msgpack"

page_header_size = [
  8, # PageXLogRecPtr pd_lsn;
  2, # uint16		pd_checksum;
  2, # uint16		pd_flags;
  2, # LocationIndex pd_lower;
  2, # LocationIndex pd_upper;
  2, # LocationIndex pd_special;
  2, # uint16		pd_pagesize_version;
  4, # TransactionId pd_prune_xid;
].sum

def parse_page_header(data)
  names = [
    "lsn_xlogid",
    "lsn_xrecoff",
    "checksum",
    "flags",
    "lower",
    "upper",
    "special",
    "pagesize_version",
    "prune_xid",
  ]
  Hash[names.zip(data.unpack("LLSSSSSSL"))]
end

def parse_meta_page_special(data)
  case data.bytesize
  when 12
    Hash[["next", "max", "version"].zip(data.unpack("LLC"))]
  else
    Hash[["next", "max", "version", "end"].zip(data.unpack("LLLL"))]
  end
end

block_size = 8192
unpacker = MessagePack::Unpacker.new
i = 0
while page = ARGF.read(block_size)
  header = parse_page_header(page.byteslice(0, page_header_size))
  if i == 0
    puts("Meta:")
    puts("  Header:")
    header.each do |key, value|
      puts("    #{key}: #{value}")
    end
    meta_page_special = page.byteslice(header["special"],
                                       block_size - header["special"])
    parse_meta_page_special(meta_page_special).each do |key, value|
      puts("  #{key}: #{value}")
    end
  else
    puts("Page#{i}")
    header.each do |key, value|
      puts("  #{key}: #{value}")
    end
    data = page.byteslice(page_header_size,
                          header["lower"] - page_header_size)
    unpacker.feed_each(data) do |object|
      puts(PP.pp(object, ""))
    end
  end
  i += 1
end
