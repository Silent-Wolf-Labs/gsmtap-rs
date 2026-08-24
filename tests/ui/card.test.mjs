import { addCardItem, createCard } from '../../static/components/cards/card.js';

test('creates a titled card and accessible metric item', () => {
  const card = createCard({ title: 'Status', className: 'status-card' });
  addCardItem(card, 'Received', 8, '', 'Total received packets.');
  expect(card.className).toBe('card status-card');
  expect(card.querySelector('h2').textContent).toBe('Status');
  expect(card.querySelector('.card-item').title).toBe('Total received packets.');
});
