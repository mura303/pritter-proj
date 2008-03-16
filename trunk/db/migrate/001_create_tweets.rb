class CreateTweets < ActiveRecord::Migration
  def self.up
    create_table :tweets do |t|
      t.column :name, :string
      t.column :content, :string
      t.column :created_on, :time
      t.column :profile_id, :integer
    end
  end

  def self.down
    drop_table :tweets
  end
end
