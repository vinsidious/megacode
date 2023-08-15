#!/usr/bin/env ruby
# encoding: utf-8
# ruby: 2.1
=begin
this script will open a AM raw audio file gerenate by rtl_fm and decode the megacode message from it
=end

# constants
RATE = 24000 # the output sample rate, in Hz
# the expected samples are little endian signed 16 bits intergers
THRESHOLD = ((2**16)/2)*0.5
TOLERANCE = 1.10 # how much deviation to accept

raise "provide raw AM file to decode as argument" unless ARGV[0] and File.exist? ARGV[0] and File.file? ARGV[0]

raw = IO.binread ARGV[0] # read raw file
samples = raw.unpack "s<*" # get samples (little endian signed 16 bits intergers)

# detect falling edges, after crossing the threshold
on = false # has the threshold been crossed
edges = []
samples.each_index do |i|
  sample = samples[i]
  if !on then # detect when threshold is crossed
    if sample > THRESHOLD then
      on = true
      edges << {:sample => i, :rising => true}
    end
  else
    if sample < THRESHOLD then
      on = false
      edges << {:sample => i, :rising => false}
    end
  end
end

edges.collect!{|edge| {:ms => edge[:sample]/(RATE/1000.0), :rising => edge[:rising]}} # convert edges to milliseconds
# the bursts (HF activity) should last 1ms
# verify if this is true, and ignore oscilastion within this 1ms
pulses = [] # one transmission has 24 pulses
pulse_begin = nil # first rising edge
pulse_end = nil # last falling edge
edges.each do |edge|
  raise "nil edge" unless edge
  # search first pulse (rising edge)
  unless pulse_begin then
    next unless edge[:rising]
    pulse_begin = edge
  end
  # detect pulses: falling and rising edge within 1ms
  # ignore edges within this 1ms
  if !edge[:rising] then
    pulse_end ||= edge
    if pulse_end[:ms]-pulse_begin[:ms]<=1*TOLERANCE then
      pulse_end = edge
    else # this is too long for a pulse. discard it
      pulse_begin = nil
    end
  else # rising edge
    if edge[:ms]-pulse_begin[:ms]>1*TOLERANCE then # this is the beginning of the next pulse
      raise "two rising egdes without falling edge detected" unless pulse_end # this should not happen
      pulses << pulse_begin
      pulse_begin = edge
      pulse_end = nil
    end # ignore rising egdes within a pulse
  end
end
# add last pulse
pulses << pulse_begin if pulse_begin and pulse_end
    
# split pulses into groups
# one group has 24 pulses with a bitframe of 6ms
# a blank bitframe without pulse separates groups
# we will split groups when no pulse occured within after 2 bitframes
groups = []
previous_pulse = nil
group = []
pulses.each do |pulse|
  if previous_pulse then
    group << previous_pulse
    if (pulse[:ms]-previous_pulse[:ms])>=2*6*(1-(TOLERANCE-1)) then
      groups << group
      group = []
    end
  end
  previous_pulse = pulse
end
# add last pulse
group << pulses[-1]
groups << group

# transmissions have 24 pulses
transmissions = groups.select {|group| group.size==24}

# verify that there is exactly 24 times one pulse per 6ms bitframe
# the pulse is either after 2 ms or 5 ms
values = []
transmissions.each_index do |transmission_i|
  transmission = transmissions[transmission_i]
  # use the previous pulse to sync
  sync = transmission[0][:ms]-3 # the first pulse is always in the second halt (after 5 ms)
  bits = []
  transmission.each_index do |pulse_i|
    pulse = transmission[pulse_i]
    # the next pulse is after 6 or 9 ms
    offset = pulse[:ms]-sync
#    puts "transmission #{transmission_i}, pulse #{pulse_i}, offset: #{offset} ms"
    if offset>-1.5 and offset<=1.5 then
      bits << 0
      sync = pulse[:ms]+6
    elsif offset>3-1.5 and offset<=3+1.5 then
      bits << 1
      sync = pulse[:ms]-3+6
    else
      puts "could not decode bit on transmission #{transmission_i} pulse #{pulse_i}"
      break
    end
  end
  # if there are 24 bit, decode
  if bits.size==24 then
    value = 0
    bits.each do |bit| 
      value = (value << 1) + bit
    end
    values << value
  else
#    puts "found #{bits.size} bits"
  end
end

# print results
puts "# egdes: #{edges.size}"
puts "# pulses: #{pulses.size}"
puts "# groups: #{groups.size} (#{groups.collect{|group| group.size}*', '})"
puts "# transmissions: #{transmissions.size}"
puts "# values: #{values.size}"
unless values.empty? then
  puts "values: "
  values.each do |value|
    button = value & 7
    code = (value >> 3) & 65535
    facility = (value >> 19) & 15
    printf("- value: 0X%06x, code: %05d, facility: %d, button: %d\n", value, code, facility, button)
  end
end
