// Asks for the access token when the server requires one (doc/ui-design-plan.md, section 8.2).

import { API_PREFIX } from '@glcm/api';
import { Button, Group, Modal, PasswordInput, Stack, Text } from '@mantine/core';
import { useState, type FormEvent } from 'react';
import { useAuth } from '../api/auth';
import { queryClient } from '../api/queryClient';

export function TokenPrompt() {
  const open = useAuth((state) => state.promptOpen);
  const [value, setValue] = useState('');
  const [error, setError] = useState<string | null>(null);
  const [checking, setChecking] = useState(false);

  const submit = async (event: FormEvent) => {
    event.preventDefault();
    const token = value.trim();
    if (!token) {
      setError('Enter the access token.');
      return;
    }
    setChecking(true);
    setError(null);
    try {
      // Checked with a request of its own, so a wrong token does not reopen the prompt in a loop
      const response = await fetch(`${API_PREFIX}/catalog`, { headers: { authorization: `Bearer ${token}`, accept: 'application/json' } });
      if (response.status === 401) {
        setError('The server did not accept this token.');
        return;
      }
      if (!response.ok) {
        setError(`The server answered with status ${response.status}.`);
        return;
      }
      useAuth.getState().setToken(token);
      setValue('');
      await queryClient.invalidateQueries();
    } catch {
      setError('The server could not be reached.');
    } finally {
      setChecking(false);
    }
  };

  return (
    <Modal opened={open} onClose={() => undefined} withCloseButton={false} closeOnClickOutside={false} closeOnEscape={false} title="Access token" centered>
      <form onSubmit={submit}>
        <Stack gap="sm">
          <Text size="sm">This server requires an access token. Ask its administrator for it (the server's GLCM_API_TOKEN).</Text>
          <PasswordInput
            label="Access token"
            value={value}
            onChange={(event) => setValue(event.currentTarget.value)}
            error={error}
            autoComplete="off"
            data-autofocus
          />
          <Text size="xs" c="dimmed">
            The token is kept only for this browser tab and sent with every request to the server.
          </Text>
          <Group justify="flex-end">
            <Button type="submit" loading={checking}>
              Continue
            </Button>
          </Group>
        </Stack>
      </form>
    </Modal>
  );
}
