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
}, { 
$project: { 
beneficiaries: 1, 
depth: 1, 
is_root: { 
"$cond": { 
if: { 
$eq: ["$depth", 0] 
}, 
then: 1, 
else: 0 
} 
} 
} 
}, 
{ 
$unwind: "$beneficiaries" 
}, { 
$group: { 
_id: "$beneficiaries.account", 
total: { 
$sum: 1 
}, 
total_posts: { 
$sum: "$is_root" 
} 
} 
}, { 
$project: { 
total: 1, 
total_posts: 1, 
total_comments: { 
$subtract: ["$total", "$total_posts"] 
} 
} 
} 
])