#!/usr/bin/env python
import json
import logging
import os
import re
import sys
import time

import pickledb

from contextlib import suppress

from steem import Steem
from steem.account import Account
from steem.blockchain import Blockchain
from steem.post import Post
from steembase.exceptions import PostDoesNotExist

STEEM_NODES = [
#    'http://127.0.0.1:8090'
]

FILTERED_OPERATIONS = ["vote","comment","transfer","transfer_to_vesting"]
BOT_AUTHOR_THRESHOLD = 5
BOT_VOTER_THRESHOLD = 5
BOT_TRANSFER_THRESHOLD = 5

def process_memo(db, operation):
    logging.info("Processing trasfer memo %s", operation['memo'])

def process_operation(db, operation):
    if operation['type'] == "account_create":
        db.set(operation['name'], operation['timestamp'])
    elif operation['type'] == "vote":
        if db.get(operation['voter']):
            db.set(operation['voter'], db.get(operation['voter']) + 1)
        else:
            db.set(operation['voter'], 1)
    elif operation['type'] == "comment":
        if db.get(operation['author']):
            db.set(operation['author'], db.get(operation['author']) + 1)
        else:
            db.set(operation['author'], 1)
    elif operation['type'] == "transfer":
        if db.get(operation['to']):
            db.set(operation['to'], db.get(operation['to']) + 1)
        else:
            db.set(operation['to'], 1)

        process_memo(db, operation)
    elif operation['type'] == "transfer_to_vesting":
        if db.get(operation['to']):
            db.set(operation['to'], db.get(operation['to']) + 1)
        else:
            db.set(operation['to'], 1)

def fill(databases, chain, first_block, last_block):
    for operation in chain.history(start_block = first_block, end_block = last_block):
        logging.info("Processing operation %s at block %s", operation['type'], operation['block_num'])
        if operation['type'] in FILTERED_OPERATIONS:
            process_operation(databases[operation['type']], operation)

def analyze(databases, result):
    for account in databases['account_create'].getall():
        if (databases['vote'].get(account) < BOT_VOTER_THRESHOLD & databases['comment'].get(account) > BOT_AUTHOR_THRESHOLD):
            result.set(account, "comment_bot")
        elif databases['vote'].get(account) >= BOT_VOTER_THRESHOLD &(databases['transfer'].get(account) <= BOT_TRANSFER_THRESHOLD | datbases['transfer_to_vesting'].get(account) <= BOT_TRANSFER_THRESHOLD) & databases['comment'].get(account) < BOT_AUTHOR_THRESHOLD:
            result.set(account, "voter_bot")

def main():
    account_db = pickledb.load("steem_accounts.db", False)
    votes_db = pickledb.load('steem_account_votes.db', False)
    comments_db = pickledb.load('steem_account_comments.db', False)
    transfers_db = pickledb.load('steem_account_trasnfers.db', False)
    vesting_transfers_db = pickledb.load('steem_account_vesting_transfers.db', False)
    result_db = pickledb.load('result.db', False)

    steemd = Steem(STEEM_NODES)
    chain = Blockchain(steemd_instance = steemd, mode = 'head')

    logging.getLogger().setLevel(20)

    databases = {'account_create': account_db,
                 'vote': votes_db,
                 'comment': comments_db,
                 'transfer': transfers_db,
                 'transfer_to_vesting': vesting_transfers_db
                }

    fill(databases, chain, first_block = round((1497970800 - 1451606400) / 3), last_block = chain.info()['head_block_number'])

    votes_db.dump()
    comments_db.dump()
    transfers_db.dump()
    vesting_transfers_db.dump()

    analyze(databases, result_db)

    result_db.dump()

if __name__ == "__main__":
    main()
