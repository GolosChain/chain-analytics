db.comment_object.aggregate([

		{$match: 
		  {
		  "last_payout":
		  	{"$gte":"2018-05-01T00:00:00","$lt":"2018-06-01T00:00:00"},
		  "total_payout_value":{"$gte":0.001}}}, 
		  
		  {$group:
		    
		    {
		      _id: { $substr: ["$last_payout", 0, 10]},
		      author_gbg:{$sum:"$total_payout_value"},
		      author_golos:{$sum:{$multiply:["$author_rewards",0.001]}},
		      beneficiary_gbg:{$sum:"$beneficiary_payout_value"},
		      curator_gbg:{$sum:"$curator_payout_value"},
		      total_gbg:{"$sum":{"$add":["$beneficiary_payout_value","$total_payout_value","$curator_payout_value"]}},
		      total_golos:{"$sum":{$multiply:[{"$add":["$beneficiary_payout_value","$total_payout_value","$curator_payout_value"]},
		      {$divide:["$author_rewards","$total_payout_value"]},0.001]
		     }}}}])