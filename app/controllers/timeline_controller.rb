class TimelineController < ApplicationController

  def anybody
    @prifileid = nil
    @pages, @tweets = paginate( :tweet, :per_page => 20, :order => "id DESC" )
    @profiles = Profile.find_all
  end

  def people
    @profile = Profile.find(params[:id])
    @latest_tweet = @profile.tweets.find( :first, :order => "id DESC" )
    @pages, @tweets = paginate( :tweet, :per_page => 20, :conditions => ['profile_id = ?', @profile.id ], :order => "id DESC" )
    @profiles = Profile.find_all
  end

  def single
    @tweet = Tweet.find( params[:id] )
  end

  def rss
    render_rss( params[:id] )
  end

  def update

    dummy_jid = 'nanashi'
    message = params[:status] || 'no status content'
        
    profile = Profile.find_or_create_by_jid( dummy_jid )

    @tweet = Tweet.create!( :name => dummy_jid, :content => Tweet.url2link_of_body(message), :profile => profile )
    profile.tweetcount += 1

    FileUtils.copy_file( "#{RAILS_ROOT}/public/images/profile/default.jpg",
    "#{RAILS_ROOT}/public/images/profile/#{profile.id}.jpg" )

    profile.save!

    render_text message
    
  end

  def render_rss( id )
    require 'rss'
    require 'cgi'

    output = RSS::Maker.make("1.0") do |maker|
      maker.channel.about = url_for( :action => 'anybody' )
      maker.channel.title = "Pritter feed"
      maker.channel.link = url_for( :action => 'anybody' )
      maker.channel.description = "Pritter feed"

      tweets = nil
      
      if id
        tweets = Tweet.find( :all, :conditions => ['profile_id = ?', id], :limit => 50, :order => 'id DESC' )
      else
        tweets = Tweet.find( :all, :limit => 50, :order => 'id DESC' )
      end

      tweets.each do |tweet|

        item = maker.items.new_item
        item.title = "#{tweet.profile.jid}: #{tweet.content}"
        item.description = "#{tweet.profile.jid}: #{tweet.content}"
        item.content_encoded = "#{tweet.profile.jid}: #{tweet.content}"
        item.date = tweet.created_on
        item.link = url_for( :action => 'anybody' )
      end
      
    end
    
    @headers["Content-Type"] = 'application/xml; charset=UTF-8'

    render :text => output.to_s, :layout => false
  end

end
