require 'xmpp4r/client'
require 'xmpp4r/vcard/helper/vcard'
require 'xmpp4r-simple'
require 'fileutils'

def save_jpeg( id, base64str )
  File.open( "#{RAILS_ROOT}/public/images/profile/#{id}.jpg", "w" ) do |f|
    f.write base64str.unpack('m')
  end
end


class Messenger

  include Jabber

  def self.runreceiver( user, pass )
    Jabber::debug = true

    im = Jabber::Simple.new( user, pass )
    vcard_helper = Vcard::Helper.new(im.client)

    loop do 
      im.received_messages do |msg|
        profile = Profile.find_or_create_by_jid( msg.from.node )

        tweet = Tweet.create!( :name => msg.from.node, :content => Tweet.url2link_of_body(msg.body), :profile => profile )
        profile.tweetcount += 1

        vcard = vcard_helper.get( msg.from.strip ) or {} # force vcard to be a Hash object, not nil

        profile.fullname = vcard['FN'] or profile.jid
        profile.url = vcard['URL'] or 'none'
        if vcard['PHOTO/BINVAL']
          save_jpeg( profile.id, vcard['PHOTO/BINVAL'] )
        else
          FileUtils.copy_file( "#{RAILS_ROOT}/public/images/profile/default.jpg",
                               "#{RAILS_ROOT}/public/images/profile/#{profile.id}.jpg" )
        end

        profile.save!

      end
      sleep 1
    end

  end


end
