class SetDummyTimeStumpToAllTweets < ActiveRecord::Migration
  def self.up
    Tweet.find( :all ).each do |tweet|
      tweet.created_on = Time.now
      tweet.save!
    end
  end

  def self.down
  end
end
