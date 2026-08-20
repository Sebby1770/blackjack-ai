/* Blackjack-ai web engine: shoe, Hi-Lo (hidden hole), basic strategy, EV.
   Works in the browser (BlackjackWeb) and Node (module.exports). */
(function (root) {
  "use strict";

  const RANKS = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13];
  const SUITS = ["♠", "♥", "♦", "♣"];

  function cardValue(rank) {
    if (rank === 1) return 11;
    if (rank >= 10) return 10;
    return rank;
  }

  function hiLo(rank) {
    const v = cardValue(rank);
    if (v >= 2 && v <= 6) return 1;
    if (v >= 7 && v <= 9) return 0;
    return -1;
  }

  function cardLabel(card) {
    const names = { 1: "A", 11: "J", 12: "Q", 13: "K" };
    return (names[card.rank] || String(card.rank)) + card.suit;
  }

  function makeShoe(decks, rng) {
    const cards = [];
    for (let d = 0; d < decks; d++) {
      for (const suit of SUITS) {
        for (const rank of RANKS) cards.push({ rank, suit });
      }
    }
    for (let i = cards.length - 1; i > 0; i--) {
      const j = Math.floor(rng() * (i + 1));
      const t = cards[i];
      cards[i] = cards[j];
      cards[j] = t;
    }
    return { cards, i: 0, running: 0, decks };
  }

  function deal(shoe, counted) {
    if (shoe.i >= shoe.cards.length) {
      const fresh = makeShoe(shoe.decks, Math.random);
      shoe.cards = fresh.cards;
      shoe.i = 0;
      shoe.running = 0;
    }
    const c = shoe.cards[shoe.i++];
    if (counted) shoe.running += hiLo(c.rank);
    return c;
  }

  function countCard(shoe, card) {
    shoe.running += hiLo(card.rank);
  }

  function handValue(cards) {
    let total = 0;
    let aces = 0;
    for (const c of cards) {
      total += cardValue(c.rank);
      if (c.rank === 1) aces += 1;
    }
    while (total > 21 && aces > 0) {
      total -= 10;
      aces -= 1;
    }
    return { total, soft: aces > 0 && total <= 21 };
  }

  function isBlackjack(cards) {
    return cards.length === 2 && handValue(cards).total === 21;
  }

  function basicAction(total, dealerUp, soft, canDouble, canSurrender) {
    const d = dealerUp;
    if (canSurrender && !soft) {
      if (total === 16 && (d === 9 || d === 10 || d === 11)) return "surrender";
      if (total === 15 && d === 10) return "surrender";
    }
    const dbl = (alt) => (canDouble ? "double" : alt);
    if (soft) {
      if (total >= 19) return "stand";
      if (total === 18) {
        if (d >= 3 && d <= 6) return dbl("stand");
        if (d === 2 || d === 7 || d === 8) return "stand";
        return "hit";
      }
      if (total === 17) return d >= 3 && d <= 6 ? dbl("hit") : "hit";
      if (total === 16 || total === 15) return d >= 4 && d <= 6 ? dbl("hit") : "hit";
      if (total === 14 || total === 13) return d >= 5 && d <= 6 ? dbl("hit") : "hit";
      return "hit";
    }
    if (total >= 17) return "stand";
    if (total >= 13 && total <= 16) return d >= 2 && d <= 6 ? "stand" : "hit";
    if (total === 12) return d >= 4 && d <= 6 ? "stand" : "hit";
    if (total === 11) return dbl("hit");
    if (total === 10) return d >= 2 && d <= 9 ? dbl("hit") : "hit";
    if (total === 9) return d >= 3 && d <= 6 ? dbl("hit") : "hit";
    return "hit";
  }

  const P_TEN = 4 / 13;
  const P_RANK = 1 / 13;

  function addCardTotal(total, soft, v) {
    total += v;
    if (v === 11) soft = true;
    if (total > 21 && soft) {
      total -= 10;
      soft = false;
    }
    return { total, soft };
  }

  function dealerFrom(total, soft, h17, memo) {
    if (total > 21) return [0, 0, 0, 0, 0, 1];
    if (total > 17 || (total === 17 && !(h17 && soft))) {
      const p = [0, 0, 0, 0, 0, 0];
      p[total - 17] = 1;
      return p;
    }
    const key = total + (soft ? "s" : "h") + (h17 ? "H" : "S");
    if (memo[key]) return memo[key];
    const p = [0, 0, 0, 0, 0, 0];
    const mix = (v, w) => {
      const n = addCardTotal(total, soft, v);
      const q = dealerFrom(n.total, n.soft, h17, memo);
      for (let i = 0; i < 6; i++) p[i] += w * q[i];
    };
    for (let v = 2; v <= 9; v++) mix(v, P_RANK);
    mix(10, P_TEN);
    mix(11, P_RANK);
    memo[key] = p;
    return p;
  }

  function vsDealer(player, dist) {
    if (player > 21) return -1;
    let ev = dist[5];
    for (let t = 17; t <= 21; t++) {
      const pt = dist[t - 17];
      if (player > t) ev += pt;
      else if (player < t) ev -= pt;
    }
    return ev;
  }

  function infiniteDeckEV(playerTotal, dealerUp, soft, rules) {
    rules = rules || {};
    const dist = dealerFrom(dealerUp, dealerUp === 11, !!rules.h17, {});
    const stand = vsDealer(playerTotal, dist);
    let hit = 0;
    const take = (v, p) => {
      const n = addCardTotal(playerTotal, soft, v);
      hit += p * (n.total > 21 ? -1 : vsDealer(n.total, dist));
    };
    for (let v = 2; v <= 9; v++) take(v, P_RANK);
    take(10, P_TEN);
    take(11, P_RANK);
    return {
      stand,
      hit,
      double: rules.allowDouble === false ? stand : 2 * hit,
      surrender: rules.allowSurrender === false ? stand : -0.5,
    };
  }

  function mulberry32(a) {
    return function () {
      a |= 0;
      a = (a + 0x6d2b79f5) | 0;
      let t = Math.imul(a ^ (a >>> 15), 1 | a);
      t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t;
      return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
    };
  }

  function trueCount(shoe) {
    const left = shoe.cards.length - shoe.i;
    const decks = Math.max(1, left / 52);
    return shoe.running / decks;
  }

  function canSplit(cards) {
    return !!cards && cards.length === 2 && cardValue(cards[0].rank) === cardValue(cards[1].rank);
  }

  function betUnits(tc) {
    if (tc < 1) return 1;
    if (tc < 2) return 2;
    if (tc < 3) return 5;
    if (tc < 4) return 9;
    return 12;
  }

  function penetration(shoe) {
    if (!shoe.cards.length) return 0;
    return shoe.i / shoe.cards.length;
  }

  function vsOutcome(playerTotal, dealerTotal, bet) {
    if (playerTotal > 21) return -bet;
    if (dealerTotal > 21) return bet;
    if (playerTotal > dealerTotal) return bet;
    if (playerTotal < dealerTotal) return -bet;
    return 0;
  }

  const api = {
    cardValue,
    hiLo,
    cardLabel,
    makeShoe,
    deal,
    countCard,
    handValue,
    isBlackjack,
    basicAction,
    infiniteDeckEV,
    mulberry32,
    trueCount,
    canSplit,
    betUnits,
    penetration,
    vsOutcome,
  };

  if (typeof module !== "undefined" && module.exports) module.exports = api;
  else root.BlackjackWeb = api;
})(typeof globalThis !== "undefined" ? globalThis : this);
