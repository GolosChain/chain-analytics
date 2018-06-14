db.comment_object.aggregate([{ 
$match: { 
$and: [{ 
"beneficiaries": { 
$exists: true 
} 
}, { 
"beneficiaries.0": { 
$exists: true 
} 
}], 
"last_payout": { 
"$gte": "2018-03-01T00:00:00", 
"$lt": "2018-06-09T00:00:00" 
} 
} 
}, 
{ 
$unwind: "$beneficiaries" 
}, { 
$group: { 
_id: "$beneficiaries.account", 

author_gbg: { 
$sum: "$total_payout_value" 
}, 
beneficiary_gbg: { 
$sum: "$beneficiary_payout_value" 
}, 
curator_gbg: { 
$sum: "$curator_payout_value" 
} 
} 
} 
])