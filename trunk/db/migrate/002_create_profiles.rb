class CreateProfiles < ActiveRecord::Migration
  def self.up
    create_table :profiles do |t|
      t.column :jid, :string
      t.column :fullname, :string
      t.column :url, :string
      t.column :tweetcount, :integer, :default=>0
    end
  end

  def self.down
    drop_table :profiles
  end
end
